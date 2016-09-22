#pragma once

#include <string>
#include <atomic>

class GithubProject {
public:

    GithubProject(std::string const & path, std::string const & url):
        files_(1),
        path_(path),
        url_(url) {
    }

    /** Called by worker when project is closed so that it can be deleted at the earliest convenience.
     */
    void close() {
        if (--files_ == 0)
            delete this;
    }

private:
    friend class Writer;
    friend class TokenizedFile;
    unsigned id_ = 0;

    std::string path_;
    std::string url_;


    std::atomic_uint files_;
};

class TokenizedFile {
public:

    TokenizedFile(GithubProject * project, std::string const & path):
        project_(project),
        path_(path) {
        ++project->files_;
    }

    ~TokenizedFile() {
        if (--project_->files_ == 0)
            delete project_;
    }


private:
    friend class Writer;
    unsigned id_ = 0;


    GithubProject * project_;
    std::string path_;

    unsigned bytes_ = 0;






};
