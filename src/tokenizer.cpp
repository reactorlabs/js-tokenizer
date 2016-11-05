#include <dirent.h>
#include <cstring>
#include <thread>
#include <fstream>

#include "tokenizer.h"
#include "merger.h"

#include "tokenizers/js.h"

//#include "tokenizers/generic.h"


unsigned fileTimestamp(std::string const & file, std::string const & path) {
    return std::atoi(exec(STR("git log --diff-filter=A --pretty=format:\"%at\" -- \"" << file << "\""), path).c_str());
}



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
    Worker::Log(STR("tokenizing " << tf->absPath()));
    JSTokenizer::tokenize(tf);

    tf->stats.createdDate = cdate;

    processedBytes_ += tf->stats.bytes();
    ++processedFiles_;

    Merger::Schedule(MergerJob(tf));

}
