#include "tokenizer.h"
#include "DBWriter.h"

#include "downloader.h"

std::string ClonedProject::path() const {
    return STR(Downloader::DownloadDir() << "/" << id);
}

void ClonedProject::release() {
    if (--handles_ == 0) {
        Downloader::release(this);
        delete this;
    }
}


std::atomic_uint Downloader::openedProjects_(0);

std::string Downloader::downloadDir_ = "tmp";
bool Downloader::keepAfterDone_ = false;


void Downloader::process(ClonedProject * const & job) {

    // go to the output directory and download the project
    if (isDirectory(STR(downloadDir_ << "/" << job->id)))
        exec(STR("rm -rf " << job->id), downloadDir_);

    // clone the project
    std::string out = exec(STR("git clone " << job->cloneUrl() << " " << job->id), downloadDir_);
    if (out.find("fatal:") != std::string::npos)
        throw (STR("Unable to clone project " << job->cloneUrl()));

    // the project has been cloned successfully
    ++openedProjects_;

    // send the project to writer, we must make sure we send it to existing writer
    job->attach(); // increase attachment to 2
    if (Writer<TokenizerType::Generic>::Statistic().started > 0)
        Writer<TokenizerType::Generic>::Schedule(WriterJob<TokenizerType::Generic>::Project(job), this);
    else
        Writer<TokenizerType::JavaScript>::Schedule(WriterJob<TokenizerType::JavaScript>::Project(job), this);

    // tokenize
    Tokenizer::Schedule(job, this);

    // if all is ok, increase the number of opened projects so that we do not stress the disk
    ++openedProjects_;
}

