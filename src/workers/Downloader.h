#pragma once

#include <memory>

#include "../helpers.h"

#include "../worker.h"

#include "DBWriter.h"
#include "Tokenizer.h"

class DownloaderJob : public std::shared_ptr<ClonedProject> {
public:
    DownloaderJob() {
    }

    DownloaderJob(ClonedProject * ci):
        std::shared_ptr<ClonedProject>(ci) {
    }

    friend std::ostream & operator << (std::ostream & s, DownloaderJob const & j) {
        s << "Project id " << j->id << ", url: " << j->cloneUrl();
        return s;
    }
};

/** Errors are projects failed to download, or failed to obrain cdates.

  jobsDone = all checked projects.
 */
class Downloader : public Worker<DownloaderJob> {
public:
    Downloader(unsigned index):
        Worker<DownloaderJob>("DOWNLOADER", index) {
    }

    static void SetDownloadDir(std::string const & value) {
        downloadDir_  = value;
        createDirectory(value);
    }

    static void Initialize() {
        createDirectory(downloadDir_);
    }

private:
    virtual void process() {
        job_->path = STR(downloadDir_ << "/" << job_->id);
        // if the directory exists, delete it first
        if (isDirectory(job_->path))
            job_->deleteFromDisk();

        // download the project using git
        std::string out = exec(STR("git clone " << job_->cloneUrl() << " " << job_->id), downloadDir_);
        if (out.find("fatal:") != std::string::npos)
            throw (STR("Unable to clone project"));

        // make git create list of all files and their create dates
        exec(STR("git log --format=\"format:%at\" --name-only --diff-filter=A > cdate.js.tokenizer.txt"), job_->path);
        if (not isFile(STR(job_->path << "/cdate.js.tokenizer.txt")))
            throw (STR("Unable to create cdate files in project"));

        // the project has been downloaded successfully, pass it to DBWriter
        DBWriter::Schedule(DBWriterJob(job_));

        // and of course, pass it to the tokenizer
        Tokenizer::Schedule(TokenizerJob(job_));

    }


    static std::string downloadDir_;
};
