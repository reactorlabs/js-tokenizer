#include "data.h"
#include "worker.h"

struct TokenizerJob {
    GitProject * project;
    std::string relPath;

    TokenizerJob(std::string const & path, std::string const & url):
        relPath("") {
    }

    TokenizerJob(TokenizerJob const & job, std::string const & dir):
        project(job.project),
        relPath(job.relPath.empty() ? dir : (job.relPath + "/" + dir)) {
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

class Tokenizer: public QueueWorker<TokenizerJob> {
public:
    Tokenizer(unsigned index):
        QueueWorker<TokenizerJob>(STR("TOKENIZER " << index)) {
    }

protected:

    void schedule(TokenizerJob const & job) {
        QueueWorker<TokenizerJob>::schedule(job);
    }

private:

    /** Looks at all javascript files in the directory and tokenizes them.

      Schedules any directories it finds recursively.
     */
    void process(TokenizerJob const & job) override;


    /** Tokenizes given file and schedules it for token identification and writing.
     */
    void tokenize(GitProject * project, std::string const & relPath);

};
