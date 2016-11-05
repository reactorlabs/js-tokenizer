#pragma once
#include "data.h"
#include "worker.h"

struct TokenizerJob {
    GitProject * project;
    std::string relPath;

    TokenizerJob(std::string const & path, std::string const & url):
        project(new GitProject(path, url)),
        relPath("") {
        ++project->handles_;
    }

    std::string absPath() const {
        if (relPath.empty())
            return project->path();
        else
            return project->path() + "/" + relPath;
    }

    /** Prettyprinting.
     */
    friend std::ostream & operator << (std::ostream & s, TokenizerJob const & job);
};

class Tokenizer: public QueueProcessor<TokenizerJob> {
public:
    Tokenizer(unsigned index):
        QueueProcessor<TokenizerJob>(STR("TOKENIZER " << index)) {
    }

    static void initializeWorkers(unsigned num);

private:

    /** Looks at all javascript files in the directory and tokenizes them.

      Schedules any directories it finds recursively.
     */
    void process(TokenizerJob const & job) override;


    /** Tokenizes given file and schedules it for token identification and writing.
     */
    void tokenize(GitProject * project, std::string const & relPath, int cdate);

};
