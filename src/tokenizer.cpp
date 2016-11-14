#include <dirent.h>
#include <cstring>
#include <thread>
#include <fstream>

#include "tokenizer.h"
#include "merger.h"

#include "db.h"


#include "tokenizers/js.h"



#include "tokenizers/generic.h"


std::atomic_uint Tokenizer::fid_(0);

std::atomic_uint Tokenizer::jsErrors_(0);

void Tokenizer::process(TokenizerJob const & job) {
    ClonedProject * p = job.project;
    // first, check if the directory already contains the file with last dates
    std::ifstream cdates(STR(p->path() << "/cdate.js.tokenizer.txt"));
    // if the file does not exist, create it first by running git
    if (not cdates.good()) {
        exec(STR("git log --format=\"format:%at\" --name-only --diff-filter=A > cdate.js.tokenizer.txt"), p->path());
        // reopen the file
        cdates.open(STR(p->path() << "/cdate.js.tokenizer.txt"));
        assert(cdates.good() and "It should really exist now");
    }
    // now that we know the project is valid, add the project to the database
    Db::addProject(p);
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
    throw STR("NOT IMPLEMENTED");
    // project bookkeeping, so that floating projects are deleted when all their files are written and they are no longer needed
    //--job.project->handles_;
}

void Tokenizer::tokenize(ClonedProject * project, std::string const & relPath, int cdate) {
    // TODO deal with different tokenizers being selectable programatically
    TokenizedFile * tf = new TokenizedFile(project, ++fid_, relPath);
    if (isFile(tf->path())) {
        Worker::Log(STR("tokenizing " << tf->path()));
        GenericTokenizer::tokenize(tf);

        // JSTokenizer::tokenize(tf);

        tf->createdDate = cdate;

        processedBytes_ += tf->bytes;
        ++processedFiles_;
        if (tf->errors > 0)
            ++jsErrors_;
        Merger::Schedule(MergerJob(tf));
    } else {
        delete tf;
    }
}
