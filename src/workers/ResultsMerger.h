#ifdef HAHA

#pragma once
#include "DBWriter.h"
#include "Writer.h"
#include "../buffers.h"



class ResultsMergerJob {
public:
    ResultsMergerJob():
        first(-1),
        second(-1) {
    }

    ResultsMergerJob(int first, int second, TokenizerKind tokenizer):
        first(first),
        second(second),
        tokenizer(tokenizer) {
        if (second < first)
            assert (second >= first);
    }

    bool isValid() const {
        return first != -1;
    }

    std::string strideName() const {
        if (first == second)
            return STR(first);
        else
            return STR(first << "_to_" << second);
    }

    ResultsMergerJob left() {
        if (first + 1 == second)
            return ResultsMergerJob(first, first, tokenizer);
        int middle = (second - first) / 2 + first;
        return ResultsMergerJob(first, middle, tokenizer);

    }

    ResultsMergerJob right() {
        if (first + 1 == second)
            return ResultsMergerJob(second, second, tokenizer);
        int middle = (second - first) / 2 + first;
        return ResultsMergerJob(middle + 1, second, tokenizer);
    }

    bool operator == (ResultsMergerJob const & other) const {
        return tokenizer == other.tokenizer and first == other.first and second == other.second;
    }

    int first;
    int second;
    TokenizerKind tokenizer;

private:

    friend std::ostream & operator << (std::ostream & s, ResultsMergerJob const & job) {
        s << "strides " << job.first << " - " << job.second << " for " << job.tokenizer << " tokenizer";
        return s;
    }
};

namespace std {

template<>
struct hash<::ResultsMergerJob> {
    std::size_t operator()(::ResultsMergerJob const & h) const {
        return std::hash<int>()(h.first) + std::hash<int>()(h.second) + std::hash<int>()(static_cast<int>(h.tokenizer));
    }
};

}



class ResultsMerger: public Worker<ResultsMergerJob> {
public:

    typedef ResultsMergerJob Job;

    /** This represents the state of a merged chunk.

      Pending means that the chunk might become available (i.e. it is not ready yet, but there is hope we can compute it.
      Done means the chunk is done and can be used.
      NotAvailable means the chunk has not yet been tokenized. Any result depending on the chunk must be not available as well.
     */
    enum class JobState {
        Pending,
        Done,
        NotAvailable,
    };

    static char const * Name() {
        return "RESULTS MERGER";
    }

    ResultsMerger(unsigned index):
        Worker<Job>(Name(), index) {
    }

private:

    static void OpenFile(std::ofstream & f, std::string const & filename) {
        f.open(filename);
        if (not f.good())
            throw STR("Unable to open output file " << filename);
    }

    static int HexChar(char x) {
        return (x >= 'a') ? ( 10 + x - 'a') : x - '0';
    }

    static std::pair<int, int> DecodeTokenMap(std::string const & item) {
        int id = 0;
        unsigned i = 0;
        while (item[i] != '@')
            id = id * 16 + HexChar(item[i++]);
        i += 6; // @@::@@
        int count = std::atoi(item.c_str() + i);
        return std::pair<int, int>(id, count);
    }

    std::string fileName(Buffer::Kind kind, Job const & job) {
        return Buffer::FileName(kind, job.tokenizer, job.strideName());
    }

    std::string tableName(Buffer::Kind kind, Job const & job) {
        return Buffer::TableName(kind, job.tokenizer, job.strideName());
    }

    void mv(Buffer::Kind kind, std::string const & suffix, Job const & job) {
        exec(STR("cp " << fileName(kind, job) << " " << fileName(kind, job) << suffix), Writer::OutputDir());
    }

    /** The tokenizer assigns different token IDs to different tokenizers, which is wasteful. Token IDs of left strides must therefore be compacted, which is the job of this methods.
     */
    void compactTokenIDs(Job const & stride);

    /** Merges token id and their texts from the left and right jobs.
     */
    void mergeTokenTexts(Job const & left, Job const & right);

    /** Merges token uses. Sums all tokens that are in both left and right.
     */
    void mergeTokenUses(Job const & left, Job const & right);

    /** Merges tokenied files. Outputs all files from first + translates those in second that are not already in first.
     */
    void mergeTokenizedFiles(Job const & left, Job const & right);

    /** Merges clone pairs. Takes all from left, then translates those on right to clone groups in left where possible and adds any new clone pairs that may happen for files in left & right.
     */
    void mergeClonePairs(Job const & left, Job const & right);

    /** Returns the oldest file id of the two.
     */
    int oldestFrom(SQLConnection & sql, int first, int second);

    /** Merges clone group information, finds new oldest files and add new clone group records where required.
     */
    void mergeCloneGroups(Job const & left, Job const & right);


    void setJobState(JobState state) {
        if (state == JobState::NotAvailable)
            std::cout << "here" << std::endl;
        if (state == JobState::Done) {
            // add to the stamp so that we know the work has been performed
            //Buffer::Get(Buffer::Kind::Stamp).append(STR("\"merger_" << job_.strideName() << "\", 1"));
        }
        Job x;
        {
            std::lock_guard<std::mutex> g(m_);
            assert(jobsState_.find(job_) != jobsState_.end());
            auto & i = jobsState_[job_];
            i.state = state;
            if (not i.waiting.isValid())
                return;
            if (--jobsState_[i.waiting].dependencies != 0)
                return;
            x = i.waiting;
        }
        Thread::Print(STR("    re-scheduling " << x << std::endl));
        Schedule(x);
    }

    void merge(Job const & left, Job const & right) {
        // if left one is a single stride, compact its token id's first
        if (left.first == left.second)
            compactTokenIDs(left);
        mergeTokenTexts(left, right);
        mergeTokenUses(left, right);
        mergeTokenizedFiles(left, right);
        mergeClonePairs(left, right);
        mergeCloneGroups(left, right);
        // check we are not leaking
        assert(tokensTranslation_.empty());
        assert(tokenizedFiles_.empty());
        assert(duplicatesSecondToFirst_.empty());
        assert(sharedCloneGroups_.empty());
        assert(groupSecondFileFirst_.empty());
        assert(groupFirstFileSecond_.empty());
    }

    JobState checkJob(Job const & job) {
        {
            std::lock_guard<std::mutex> g(m_);
            // if the job already has record, return it
            auto i = jobsState_.find(job);
            if (i != jobsState_.end())
                return i->second.state;
        }
        // otherwise if it is a single stride, check if it has been computed
        if (job.first == job.second) {
            SQLConnection sql;
            sql.query(STR("USE " << DBWriter::DatabaseName()));
            JobState result = JobState::NotAvailable;
            sql.query(STR("SHOW TABLES LIKE '" << Buffer::TableName(Buffer::Kind::Summary, job.tokenizer, job.strideName()) << "'"), [&result](unsigned cols, char ** row) {
              result = JobState::Done;
            });
            return result;
        } else {
            return JobState::Pending;
        }
    }

    void scheduleDependency(Job const & job) {
        std::lock_guard<std::mutex> g(m_);
        if (jobsState_.find(job) == jobsState_.end()) {
            Thread::Print(STR("    scheduling dependency " << job << std::endl));
            auto & i = jobsState_[job];
            i.state = JobState::Pending;
            i.waiting = job_;
            ++jobsState_[job_].dependencies;
            Schedule(job);
        }
    }


    /** Performs the merging.
     */
    void process() override {
        Thread::Print(STR("processing " << job_ << std::endl));
        if (job_.first == 0 and job_.second == 99)
            std::cout << "here" << std::endl;
        // get left and right subjobs and their states
        Job left = job_.left();
        Job right = job_.right();
        JobState lState = checkJob(left);
        JobState rState = checkJob(right);
        if (lState == JobState::NotAvailable or rState == JobState::NotAvailable) {
            // if either of the subproblems is not available there is no point in progressing, propagate the not available
            setJobState(JobState::NotAvailable);
        } else {
            if (lState == JobState::Done and rState == JobState::Done) {
                // if both subproblems are done, we can progress further
                merge(left, right);
                // upddate in the database as done and re-schedule any pending state
                setJobState(JobState::Done);
            } else {
                // otherwise schedule any that is pending
                if (lState == JobState::Pending)
                    scheduleDependency(left);
                if (rState == JobState::Pending)
                    scheduleDependency(right);
                jobsState_[job_].state = JobState::Pending;
            }
        }
    }


    /** Number of unique tokens in left, for easy translation.
     */
    int firstTokens_;

    /** Token id second -> token id in first or new token in in merged
     */
    std::unordered_map<int, int> tokensTranslation_;

    /** For each tokens hash in first -> file id in first.
     */
    std::unordered_map<Hash, int> tokenizedFiles_;

    /** second -> first

      For each tokenized file in second stride that has a tokenized file with the same tokens hash in the first stride contains the file id of the second one as key and the file id as the first one as value.
     */
    std::unordered_map<int, int> duplicatesSecondToFirst_;

    /** first -> second
        (second to first after reversed)

      Any clone group in first that also has clone group in second.
     */
    std::unordered_map<int, int> sharedCloneGroups_;

    /** second -> first

      For a clone group in second to which a single file from first should be added.
     */
    std::unordered_map<int, int> groupSecondFileFirst_;

    /** first -> second

      For clone groups in first to which a single file from second should be added.
     */
    std::unordered_map<int, int> groupFirstFileSecond_;



    struct State {
        JobState state;
        Job waiting;
        unsigned dependencies;

        State():
            dependencies(0) {
        }
    };


    static std::mutex m_;
    static std::unordered_map<Job, State> jobsState_;


};


#endif
