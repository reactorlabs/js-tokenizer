#pragma once

#include <atomic>
#include <unordered_map>

#include "data.h"
#include "worker.h"

struct MergerJob {
    TokenizedFile * file;

    MergerJob(TokenizedFile * file):
        file(file) {
    }

    friend std::ostream & operator << (std::ostream & s, MergerJob const & job) {
        s << job.file->absPath();
        return s;
    }

};

class Merger: public QueueProcessor<MergerJob> {
public:
    enum class StopClones {
        none,
        tokens,
        file
    };

    Merger(unsigned index):
        QueueProcessor<MergerJob>(STR("MERGER " << index)) {
    }

    static void initializeWorkers(unsigned num);

    static void writeGlobalTokens(std::ostream & s);

    static unsigned NumClones() {
        return numClones_;
    }

    static unsigned NumUniqueTokens() {
        std::lock_guard<std::mutex> g(accessM_);
        return uniqueTokenIds_.size();
    }

    static unsigned NumEmptyFiles() {
        return numEmptyFiles_;
    }

    static unsigned NumErrorFIles() {
        return numErrorFiles_;
    }

private:

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

    };

    CloneInfo checkClones(TokenizedFile * tf);

    /** Changes the tokens in the file into global identifiers.
     */
    void idsForTokens(TokenizedFile * tf);


    void process(MergerJob const & job) override;



    static StopClones stopClones_;
    static std::unordered_map<std::string, CloneInfo> clones_;


    static std::unordered_map<std::string, TokenInfo> uniqueTokenIds_;


    static std::mutex accessM_;

    static std::atomic_uint numClones_;

    static std::atomic_uint numEmptyFiles_;

    static std::atomic_uint numErrorFiles_;

};
