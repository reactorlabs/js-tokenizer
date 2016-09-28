#include <dirent.h>
#include <cstring>
#include <fstream>
#include <thread>

#include "crawler.h"
#include "tokenizer.h"


void Crawler::initializeWorkers(unsigned num) {
    for (unsigned i = 0; i < num; ++i) {
        std::thread t([i] () {
            Crawler c(i);
            c();
        });
        t.detach();
    }
}


void Crawler::process(CrawlerJob const & job) {
    std::string url = projectUrl(job.path);
    // if the directory is not empty, create job for the tokenizer
    if (not url.empty()) {
        Tokenizer::Schedule(TokenizerJob(job.path, url));
    // otherwise recursively scan all its subdirectories
    } else {
        struct dirent * ent;
        DIR * d = opendir(job.path.c_str());
        assert(d != nullptr);
        while ((ent = readdir(d)) != nullptr) {
            // if it is file or symlink, ignore
            if (ent->d_type == DT_REG or ent->d_type == DT_LNK)
                continue;
            // skip . and ..
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            // skip .git as we do not crawl it
            if (strcmp(ent->d_name, ".git") == 0)
                continue;
            // crawl the directory recursively
            schedule(CrawlerJob(job.path + "/" + ent->d_name));
        }
        closedir(d);
    }
}

std::string Crawler::projectUrl(std::string const & path) {
    std::ifstream x(path + "/.git/config");
    // if not found, it is not git repository
    if (not x.good())
        return "";
    // parse the config file
    std::string l;
    while (not x.eof()) {
        x >> l;
        if (l == "[remote") {
            x >> l;
            if (l == "\"origin\"]") {
                x >> l;
                if (l == "url") {
                    x >> l;
                    if (l == "=") {
                        x >> l;
                        return l;
                    }
                }
            }
        }
    }
    return "";
}
