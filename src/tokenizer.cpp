#include <dirent.h>
#include <cstring>

#include "tokenizer.h"

#include "tokenizers/js.h"

//#include "tokenizers/generic.h"


std::ostream & operator << (std::ostream & s, TokenizerJob const & job) {
    s << job.absPath();
    return s;
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

        // if it is directory, schedule for tokenization
        } else if (ent->d_type == DT_DIR) {
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            // skip .git as we do not crawl it
            if (strcmp(ent->d_name, ".git"))
                continue;
            // schedule the job of tokenizing the directory
            schedule(TokenizerJob(job, ent->d_name));
        }
    }
    closedir(d);
    // TODO deal with project bookkeeping as well
}

void Tokenizer::tokenize(GitProject * project, std::string const & relPath) {
    // TODO deal with different tokenizers being selectable programatically
    TokenizedFile * tf = new TokenizedFile(project, relPath);
    JSTokenizer::tokenize(tf);




}
