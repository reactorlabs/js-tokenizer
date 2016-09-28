#include <thread>

#include "merger.h"
#include "writer.h"

std::atomic_uint Merger::fid_(FILE_ID_STARTS_AT);
std::atomic_uint Merger::pid_(PROJECT_ID_STARTS_AT);


void Merger::initializeWorkers(unsigned num) {
    for (unsigned i = 0; i < num; ++i) {
        std::thread t([i] () {
            Merger c(i);
            c();
        });
        t.detach();
    }
}




void Merger::process(MergerJob const & job) {
    bool writeProject = false;
    TokenizedFile * tf = job.file;
    tf->setId(fid_++);
    if (tf->pid() == 0) {
        tf->setPid(pid_++);
        writeProject = true;
    }
    Writer::Schedule(WriterJob(job.file, writeProject));
}
