#include <dirent.h>
#include <cstring>
#include <thread>

#include "tokenizer.h"
#include "merger.h"

#include "tokenizers/js.h"

//#include "tokenizers/generic.h"

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
    struct dirent * ent;
    DIR * d = opendir(job.absPath().c_str());
    assert(d != nullptr);
    while ((ent = readdir(d)) != nullptr) {
        // ignore symlinks
        if (ent->d_type == DT_LNK)
            continue;
        // if it is file, check if it is language file and tokenize it
        if (ent->d_type == DT_REG) {
            if (isLanguageFile(ent->d_name))
                tokenize(job.project, job.relPath.empty() ? ent->d_name : (job.relPath + "/" + ent->d_name));
        // if it is directory, schedule for tokenization
        } else if (ent->d_type == DT_DIR) {
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            // skip .git as we do not crawl it
            if (strcmp(ent->d_name, ".git") == 0)
                continue;
            // schedule the job of tokenizing the directory
            schedule(TokenizerJob(job, ent->d_name));
        }
    }
    closedir(d);
    // project bookkeeping, so that floating projects are deleted when all their files are written and they are no longer needed
    --job.project->handles_;
}

void Tokenizer::tokenize(GitProject * project, std::string const & relPath) {
    // TODO deal with different tokenizers being selectable programatically
    TokenizedFile * tf = new TokenizedFile(project, relPath);
    Worker::Log(STR("tokenizing " << tf->absPath()));
    Worker::Print(STR("tokenizing " << tf->absPath()));
    JSTokenizer::tokenize(tf);

    processedBytes_ += tf->stats.bytes();
    ++processedFiles_;

    Merger::Schedule(MergerJob(tf));

}
