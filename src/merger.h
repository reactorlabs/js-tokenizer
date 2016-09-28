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
    Merger(unsigned index):
        QueueWorker<MergerJob>(STR("MERGER " << index)) {
    }

    static void initializeWorkers(unsigned num);

private:

    void process(MergerJob const & job) override;



    static std::atomic_uint fid_;
    static std::atomic_uint pid_;


};
