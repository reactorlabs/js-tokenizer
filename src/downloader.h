#pragma once

#include <atomic>

#include "worker.h"


/** Describes a downloaded project.

 */
class ClonedProject {
public:
    unsigned id;
    std::string url;
    unsigned created_at;

    ClonedProject(std::vector<std::string> const & row):
        id(std::stoi(row[0])),
        url(row[1].substr(29)),
        created_at(timestampFrom(row[6])),
        handles_(0) {
    }

    ClonedProject(unsigned id, std::string const & url, unsigned created_at):
        id(id),
        url(url),
        created_at(created_at),
        handles_(0) {
    }

    /** Returns the url to be used to clone the project.
     */
    std::string cloneUrl() const {
        return STR("http://github.com/" << url << ".git");
    }

    /** Returns the path where the project is cloned on local drive. */
    std::string path() const;

    void attach() {
        ++handles_;
    }

    void release() {
        if (--handles_ == 0) {
            // TODO also delete from disk, if this is what we want
            delete this;
        }
    }

    static std::string const & language(std::vector<std::string> const & row) {
        return row[5];
    }

    static bool isDeleted(std::vector<std::string> const & row) {
        return row[9] == "0";
    }

    static bool isForked(std::vector<std::string> const & row) {
        return row[7] != "\\N";
    }

private:

    std::atomic_uint handles_;

};


class Downloader : public QueueWorker<ClonedProject *> {
public:
    Downloader(unsigned index):
        QueueWorker<ClonedProject *>(STR("DOWNLOADER " << index)) {
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
        while (jobs_.size() + openedProjects_ > queueLimit_)
            canAdd_.wait(g);
        jobs_.push(job);
        cv_.notify_one();
    }

    static void DoneWith(ClonedProject * p) {
        --openedProjects_;
        if (jobs_.size() + openedProjects_ < queueLimit_)
            canAdd_.notify_one();
        delete p;
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

