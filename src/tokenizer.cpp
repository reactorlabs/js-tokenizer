#include <dirent.h>
#include <cstring>
#include <thread>
#include <fstream>

#include "tokenizer.h"
#include "merger.h"

#include "tokenizers/js.h"

#include "tokenizers/generic.h"




std::atomic_uint Tokenizer::jsErrors_(0);


std::ostream & operator << (std::ostream & s, TokenizerJob const & job) {
    s << job.absPath();
    return s;
}

void Tokenizer::initializeWorkers(unsigned num) {
    for (unsigned i = 0; i < num; ++i) {
        std::thread t([i] () {
            Tokenizer c(i);
            c();
        });
        t.detach();
    }
}



void Tokenizer::process(TokenizerJob const & job) {
    // first, check if the directory already contains the file with last dates
    std::ifstream cdates(STR(job.absPath() <<  "/cdate.js.tokenizer.txt"));
    // if the file does not exist, create it first
    if (not cdates.good()) {
        int result = system(STR("cd \"" << job.absPath() << "\" &&  git log --format=\"format:%at\" --name-only --diff-filter=A > cdate.js.tokenizer.txt").c_str());
        if (result != EXIT_SUCCESS)
            throw STR("Unable to get cdates for files in project directory " << job.absPath());
        // reopen the file
        cdates.open(STR(job.absPath() <<  "/cdate.js.tokenizer.txt"));
    }
    // let's parse the output now
    int date = 0;
    while (not cdates.eof()) {
        std::string tmp;
        std::getline(cdates, tmp, '\n');
        if (tmp == "") { // if the line is empty, next line will be timestamp
            date = 0;
        } else if (date == 0) { // read timestamp
            date = std::atoi(tmp.c_str());
        } else { // all other lines are actual files
            // TODO this should support multiple languages too
            if (isLanguageFile(tmp))
                tokenize(job.project, tmp, date);
        }
    }
    // project bookkeeping, so that floating projects are deleted when all their files are written and they are no longer needed
    --job.project->handles_;
}

void Tokenizer::tokenize(GitProject * project, std::string const & relPath, int cdate) {
    // TODO deal with different tokenizers being selectable programatically
    TokenizedFile * tf = new TokenizedFile(project, relPath);
    if (isFile(tf->absPath())) {
        Worker::Log(STR("tokenizing " << tf->absPath()));
        GenericTokenizer::tokenize(tf);

        // JSTokenizer::tokenize(tf);

        tf->stats.createdDate = cdate;

        processedBytes_ += tf->stats.bytes();
        ++processedFiles_;
        if (tf->stats.errors > 0)
            ++jsErrors_;

        Merger::Schedule(MergerJob(tf));
    } else {
        delete tf;
    }
}
