#include "tokenizer.h"

#include "downloader.h"

std::string ClonedProject::path() const {
    return STR(Downloader::DownloadDir() << "/" << id);
}

std::atomic_uint Downloader::openedProjects_(0);

std::string Downloader::downloadDir_ = "tmp";
bool Downloader::keepAfterDone_ = false;


void Downloader::process(ClonedProject * const & job) {
    // go to the output directory and download the project
    if (isDirectory(STR(downloadDir_ << "/" << job->id))) {
        // TODO delete the directory
    }
    // clone the project
    std::string out = exec(STR("git clone " << job->cloneUrl() << " " << job->id), downloadDir_);
    //Print(out);







    // it is cloned, so launch tokenizer
    Tokenizer::Schedule(job);

    // if all is ok, increase the number of opened projects so that we do not stress the disk
    // ++openedProjects_;
}

