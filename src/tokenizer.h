#pragma once
#include "data.h"
#include "worker.h"
#include "downloader.h"
#include "db.h"







struct TokenizerJob {
    ClonedProject * project;

    TokenizerJob(ClonedProject * project):
        project(project) {
    }

    friend std::ostream & operator << (std::ostream & s, TokenizerJob const & job) {
        s << job.project->cloneUrl();
        return s;
    }
};


class Tokenizer: public QueueWorker<TokenizerJob> {
public:
    enum class Tokenizers {
        Generic,
        JavaScript,
        GenericAndJs,
    };

    Tokenizer(unsigned index):
        QueueWorker<TokenizerJob>("TOKENIZER", index) {
    }

    static unsigned jsErrors() {
        return 0;
    }

    static void initializeWorkers(unsigned num);

private:

    /** Looks at all javascript files in the directory and tokenizes them.

      Schedules any directories it finds recursively.
     */
    void process(TokenizerJob const & job) override;


    /** Tokenizes given file and schedules it for token identification and writing.
     */
    void tokenize(ClonedProject * project, std::string const & relPath, int cdate);

    static std::atomic_uint fid_;





    static std::atomic_uint jsErrors_;

    static Tokenizers tokenizers_;

};
