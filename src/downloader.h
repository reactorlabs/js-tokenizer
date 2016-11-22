#pragma once

#include <atomic>

#include "worker.h"
#include "data.h"



class Downloader : public QueueWorker<ClonedProject *> {
public:
    Downloader(unsigned index):
        QueueWorker<ClonedProject *>("DOWNLOADER", index) {
    }


    static void Initialize() {
        createDirectory(downloadDir_);
    }

    static void SetKeepWhenDone(bool value) {
        keepAfterDone_ = value;
    }

    static void SetDownloadDir(std::string const & value) {
        downloadDir_ = value;
    }

    static std::string const & DownloadDir() {
        return downloadDir_;
    }

    /** Scheduling for downloader is a bit trickier as we have to calculate not just the queue size, but also the number of opened projects. */
    static void Schedule(ClonedProject * const & job, Worker * sender) {
        std::unique_lock<std::mutex> g(m_);
        while (jobs_.size() + openedProjects_ > queueLimit_) {
            sender->stall();
            canAdd_.wait(g);
        }
        jobs_.push(job);
        cv_.notify_one();
    }

    static void release(ClonedProject * p) {
        if (not keepAfterDone_)
            exec(STR("rm -rf " << p->id), downloadDir_);
        --openedProjects_;
        if (jobs_.size() + openedProjects_ < queueLimit_)
            canAdd_.notify_one();
    }

private:
    void process(ClonedProject * const & job) override;


    /** If true, all downloaded projects & data will be kept on the disk when they are processed. If false, all their data will be deleted once processed.
     */
    static bool keepAfterDone_;

    /** Directory into which the projects will be downloaded, each one into a folder identical to its id.
     */
    static std::string downloadDir_;

    static std::atomic_uint openedProjects_;



};

