#pragma once

#include <atomic>

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

class Merger: public QueueWorker<MergerJob> {
public:
    enum class StopClones {
        none,
        tokens,
        file
    };

    Merger(unsigned index):
        QueueWorker<MergerJob>(STR("MERGER " << index)) {
    }

    static void initializeWorkers(unsigned num);

    static void writeGlobalTokens(std::ostream & s);

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



    static unsigned fid_;
    static unsigned pid_;

    static StopClones stopClones_;
    static std::map<std::string, CloneInfo> clones_;


    static std::map<std::string, TokenInfo> uniqueTokenIds_;

    static unsigned numClones_;

};
