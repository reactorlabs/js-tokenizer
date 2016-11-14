#pragma once
#include "data.h"
#include "worker.h"
#include "downloader.h"


class TokenizedFile {
public:
    enum class Tokenizer {
        Generic,
        JavaScript,
    };

    ClonedProject * project;
    unsigned id;

    std::string relPath;

    Tokenizer tokenizer;
    unsigned totalTokens = 0;
    unsigned uniqueTokens = 0;
    unsigned errors = 0;
    unsigned createdDate = 0;
    unsigned loc = 0;
    unsigned commentLoc = 0;
    unsigned emptyLoc = 0;
    unsigned bytes = 0;
    unsigned whitespaceBytes = 0;
    unsigned commentBytes = 0;
    unsigned separatorBytes = 0;
    unsigned tokenBytes = 0;
    std::string fileHash;
    std::string tokensHash;

    std::map<std::string, unsigned> tokens;

    void addToken(std::string const & token) {
        ++totalTokens;
        tokenBytes += token.size();
        ++tokens[token];
    }

    void addSeparator(std::string const & separator) {
        separatorBytes += separator.size();
    }

    void addSeparator(unsigned size) {
        separatorBytes += size;
    }

    void addComment(std::string const & comment) {
        commentBytes += comment.size();
    }

    void addComment(unsigned size) {
        commentBytes += size;
    }

    void addWhitespace(char whitespace) {
        ++whitespaceBytes;
    }

    void newline(bool commentOnly, bool empty) {
        ++loc;
        if (commentOnly)
            if (empty)
                ++emptyLoc;
            else
                ++commentLoc;
    }

    TokenizedFile(ClonedProject * p, unsigned id, std::string relPath):
        project(p),
        id(id),
        relPath(relPath) {
    }

    std::string path() const {
        return STR(project->path() << "/" << relPath);
    }
};



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


class Tokenizer: public QueueProcessor<TokenizerJob> {
public:
    enum class Tokenizers {
        Generic,
        JavaScript,
        GenericAndJs,
    };

    Tokenizer(unsigned index):
        QueueProcessor<TokenizerJob>(STR("TOKENIZER " << index)) {
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
