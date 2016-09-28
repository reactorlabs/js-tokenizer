#pragma once

#include <dirent.h>

#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <fstream>

#include "utils.h"
#include "data.h"
#include "worker.h"

/** Writer, amongst other things writes the output.

  Writer is also responsible for keeping the global statistics.
 */
class Writer : public Worker {
public:
    Writer(unsigned index):
        Worker(STR("WRITER " << index)),
        bookkeeping(STR(PATH_OUTPUT << "/" << PATH_BOOKKEEPING_PROJS << "/bookkeeping-proj-" << index << ".txt")),
        stats(STR(PATH_OUTPUT << "/" << PATH_STATS_FILE << "/files-stats-" << index << ".txt")),
        tokens(STR(PATH_OUTPUT << "/" << PATH_TOKENS_FILE << "/files-tokens-" << index << ".txt")),
        ourdata(STR(PATH_OUTPUT << "/" << PATH_OUR_DATA_FILE << "/files-" << index << ".txt")),
        clones(STR(PATH_OUTPUT << "/" << PATH_CLONE_FILE << "/clones-" << index << ".txt")){
        if (not bookkeeping.good() or not stats.good() or not tokens.good() or not ourdata.good() or not clones.good())
            throw STR("Unable to create sourcererCC output files for writer " << index);
    }


    void run() {
        Writer::print(this, "Started...");
        while (true) {
            try {
                TokenizedFile * f = getSomethingToWrite();
                processFile(f);
            } catch (std::string const & e) {
                Writer::error(this, e);
            } catch (std::exception const & e) {
                Writer::error(this, e.what());
            } catch (...) {
                Writer::error(this, "Unknown exception raised");
            }
        }
    }

    /** Returns the number of processed files.
     */
    static unsigned long processedFiles() {
        return numFiles_;
    }

    /** Returns the number of processed projects.
     */
    static unsigned long processedProjects() {
        return numProjects_;
    }

    /** Returns the number of processed bytes.
     */
    static unsigned long processedBytes() {
        return totalBytes_;
    }

    static unsigned long errorFiles() {
        return errorFiles_;
    }

    /** Returns the number of empty files found.
     */
    static unsigned long emptyFiles() {
        return emptyFiles_;
    }

    /** Returns the number of token clones found (i.e. files that have same token hashes).
     */
    static unsigned long tokenClones() {
        return tokenClones_;
    }

    /** Returns the number of items in the writer's queue.
     */
    static unsigned queueSize() {
        std::lock_guard<std::mutex> g(queueAccess_);
        return q_.size();
    }

private:
    friend class Crawler;

    struct Record {
        unsigned count;
        unsigned pid;
        unsigned fid;

        Record():
            count(0),
            pid(0),
            fid(0) {
        }

        bool increment(unsigned pid, unsigned fid) {
            if (count == 0) {
                this->pid = pid;
                this->fid = fid;
                count = 1;
                return false;
            } else {
                ++count;
                return true;
            }
        }
    };


    void enterProcessing() {
#if NUM_WRITERS > 1
        processingAccess_.lock();
#endif
    }

    void leaveProcessing() {
#if NUM_WRITERS > 1
        processingAccess_.unlock();
#endif
    }

    static void addTokenizedFile(TokenizedFile * f) {
        std::lock_guard<std::mutex> guard(queueAccess_);
        q_.push(f);
        qCV_.notify_one();
    }

    static TokenizedFile * getSomethingToWrite() {
        std::unique_lock<std::mutex> guard(queueAccess_);
        while (q_.empty())
            qCV_.wait(guard);
        TokenizedFile * result = q_.front();
        q_.pop();
        return result;
    }

    /** Processes given file.
     */
    void processFile(TokenizedFile * f) {
        Worker::log(this, STR("processing file " << f->absPath()));

        if (f->errors_ > 0)
            ++errorFiles_;

        bool empty = f->tokens_.empty();
        bool hashClone = false;

        unsigned clonePid = 0;
        unsigned cloneFid = 0;

        // enter the processing mode in multi-writer mode
        enterProcessing();
        // if the project has not been processed, process it first
        if (f->project_->id_ == 0)
            processProject(f->project_);
        // assign file id
        f->id_ = ++numFiles_;
        // process statistics
        totalBytes_ += f->bytes_;

#if SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS == 1
        auto & i = tokenHashMatches_[f->tokensHash_]  ;
        hashClone = i.increment(f->project_->id_, f->id_);
        if (hashClone) {
            ++tokenClones_;
            clonePid = i.pid;
            cloneFid = i.fid;
        }
#endif
        if (empty)
            ++emptyFiles_;

        // leave the multi-processing mode in multi-writer mode
        leaveProcessing();
        // output file tokens and statistics

        if (not ((SOURCERERCC_IGNORE_EMPTY_FILES and empty) or
                 (SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS and hashClone))) {
            f->sourcererCCFileStats(stats);
            f->sourcererCCFileTokens(tokens);
        }

        // our data is always printed
        f->ourData(ourdata);

        // output clone information, if we have determined it is a clone
        if (hashClone)
            clones << clonePid << "," << cloneFid << "," << f->project_->id_ << "," << f->id_ << std::endl;


        // and finally, delete the file, if it is the last file in the project, deletes the project as well
        delete f;
    }

    void processProject(GithubProject * p) {
        Worker::log(this, STR("processing project " << p->path_));
        p->id_ = ++numProjects_;
        // output project stats in bookkeeping for sourcererCC
        p->sourcererCCProjectStats(bookkeeping);
    }

    std::ofstream bookkeeping;
    std::ofstream stats;
    std::ofstream tokens;
    std::ofstream ourdata;
    std::ofstream clones;


    static std::mutex queueAccess_;
    static std::queue<TokenizedFile *> q_;
    static std::condition_variable qCV_;


#if NUM_WRITERS > 1
    static std::mutex processingAccess_;
#endif

    // hashmaps for checking whether file is exact hash or not
    static std::map<std::string, Record> tokenHashMatches_;


    static volatile unsigned long numFiles_;
    static volatile unsigned long numProjects_;
    static volatile unsigned long emptyFiles_;
    static volatile unsigned long errorFiles_;
    static volatile unsigned long tokenClones_;

    static volatile unsigned long totalBytes_;


};