#pragma once

#include <atomic>
#include <unordered_map>

#include "tokenizer.h"
#include "worker.h"

struct MergerJob {
    TokenizedFile * file;

    MergerJob(TokenizedFile * file):
        file(file) {
    }

    friend std::ostream & operator << (std::ostream & s, MergerJob const & job) {
        s << job.file->path();
        return s;
    }

};

/** The merger needs to be redesigned so that it cooperates with the attached SQL database.

  But how to do the merging in the database?


 */





class Merger: public QueueProcessor<MergerJob> {
public:

    Merger(unsigned index):
        QueueProcessor<MergerJob>(STR("MERGER " << index)) {
    }

    static void writeGlobalTokens(std::ostream & s);

    static unsigned NumClones() {
        return numClones_;
    }

    static unsigned NumUniqueTokens() {
        //std::lock_guard<std::mutex> g(accessM_);
        return tokenIds_.size();
    }

    static unsigned NumEmptyFiles() {
        return numEmptyFiles_;
    }

    static unsigned NumErrorFIles() {
        return numErrorFiles_;
    }

private:

    struct TokenInfo {
        unsigned id;
        unsigned count;

        /** When new token info is created, set its count to zero and assign new id.
         */
        TokenInfo():
            id(++id_),
            count(0) {
        }

        /** Increases the number of token occurences. */
        unsigned inc() {
            ++count;
            return id;
        }

        // no need to be atomic - the map has to be locked for access anyways
        static unsigned id_;
    };


    struct CloneInfo {
        unsigned pid;
        unsigned fid;
        CloneInfo():
            pid(0),
            fid(0) {
        }

        CloneInfo(unsigned pid, unsigned fid):
            pid(pid),
            fid(fid) {
        }
    };
/*
    struct TokenInfo {
        unsigned id;
        unsigned count;

        TokenInfo(unsigned id):
            id(id),
            count(0) {
        }

        TokenInfo & operator ++ () {
            ++count;
            return *this;
        }

    }; */

    CloneInfo checkClones(TokenizedFile * tf);

    /** Changes the tokens in the file into global identifiers.
     */
    //void idsForTokens(TokenizedFile * tf);



    void lockTokenIdRead();
    void unlockTokenIdRead();
    void lockTokenIdWrite();
    void unlockTokenIdWrite();

    void lockCounts();
    void unlockCounts();


    void tokensToIds(TokenizedFile * tf);

    void process(MergerJob const & job) override;


    static std::atomic_uint fid_;
    static unsigned pid_;

    static std::unordered_map<std::string, CloneInfo> clones_;


    static std::unordered_map<std::string, unsigned> tokenIds_;

    static std::vector<unsigned> tokenCounts_;


    //static std::unordered_map<std::string, TokenInfo> uniqueTokenIds_;


    /** Mutex guarding access to clone database.
     */
    static std::mutex accessC_;

    static std::mutex accessTid_;

    static std::mutex accessTc_;

    /** Project id settings mutex guard */
    static std::mutex accessPid_;

    static std::atomic_uint numClones_;

    static std::atomic_uint numEmptyFiles_;

    static std::atomic_uint numErrorFiles_;

};
