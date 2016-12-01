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
    static char const * Name() {
        return "DOWNLOADER";
    }

    Downloader(unsigned index):
        Worker<DownloaderJob>(Name(), index) {
    }

    static void SetDownloadDir(std::string const & value) {
        downloadDir_  = value;
        createDirectory(value);
    }

    static void Initialize() {
        createDirectory(downloadDir_);
    }

    static void FlushBuffers() {
        projects_.flush();
        projectsExtra_.flush();
    }

private:
    virtual void process() {
        job_->path = STR(downloadDir_ << "/" << job_->id);
        // TODO THIS IS SUPER STUPID  -- DELETE ME
        /*
        if (not isDirectory(job_->path)) {
            //Log("Unable to clone project");
            return;
        }

        // get the commit number
        job_->commit = exec("git rev-parse HEAD", job_->path);

        // and of course, pass it to the tokenizer
        Tokenizer::Schedule(TokenizerJob(job_));

        // pass the project to the DB writer.
        projects_.append(STR(
            job_->id <<
            ",NULL," <<
            escape(job_->url)));
        projectsExtra_.append(STR(
            job_->id << "," <<
            job_->createdAt << "," <<
            escape(job_->commit)));

        return;
        */
        // NORMAL CODE GOES ON HERE

        // if the directory exists, delete it first
        if (isDirectory(job_->path))
            job_->deleteFromDisk();

        // download the project using git
        std::string out = exec(STR("GIT_TERMINAL_PROMPT=0 git clone " << job_->cloneUrl() << " " << job_->id), downloadDir_);
        if (out.find("fatal:") != std::string::npos) {
            //Log("Unable to clone project");
            ++errors_;
            return;
        }

        // make git create list of all files and their create dates
        exec(STR("git log --format=\"format:%at\" --name-only --diff-filter=A > cdate.js.tokenizer.txt"), job_->path);
        if (not isFile(STR(job_->path << "/cdate.js.tokenizer.txt")))
            throw (STR("Unable to create cdate files in project"));

        // get the commit number
        job_->commit = exec("git rev-parse HEAD", job_->path);
        if (job_->commit.substr(0, 6) == "fatal:")
            job_->commit = "0000000000000000000000000000000000000000";
        else
            job_->commit = job_->commit.substr(0,40); // get rid of the trailing \n
        // and of course, pass it to the tokenizer
        Tokenizer::Schedule(TokenizerJob(job_));

        // pass the project to the DB writer.
        projects_.append(STR(
            job_->id <<
            ",NULL," <<
            escape(job_->url)));
        projectsExtra_.append(STR(
            job_->id << "," <<
            job_->createdAt << "," <<
            escape(job_->commit)));
    }


    static std::string downloadDir_;

    static DBBuffer projects_;
    static DBBuffer projectsExtra_;
};
