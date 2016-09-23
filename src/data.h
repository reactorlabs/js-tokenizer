#pragma once

#include <cassert>
#include <string>
#include <map>
#include <atomic>

#include "hashes/md5.h"

#include "utils.h"

class GithubProject {
public:

    GithubProject(std::string const & path, std::string const & url):
        files_(1),
        path_(path),
        url_(url) {
    }

    std::string githubUrl() {
        std::string url = url_;
        if (url.substr(url.size() - 4) != ".git")
            return "";
        if (url.substr(0, 15) == "git@github.com:") {
            url = url.substr(15, url.size() - 19);
        } else if (url.substr(0, 17) == "git://github.com/") {
            url = url.substr(17, url.size() - 21);
        } else if (url.substr(0, 19) == "https://github.com/") {
            url = url.substr(19, url.size() - 23);
        } else if (url.substr(0, 34) == "https://jakubzitny:asd@github.com/") {
            url = url.substr(34, url.size() - 38);
        } else {
            return url_; // not a github project, return project url
        }
        return "https://github.com/" + url;
    }

    /** Outputs the project statistics in the sourcererCC's format.

      This is project id, % escaped path and % escaped github url, separated by commas.
     */
    void sourcererCCProjectStats(std::ostream & f) {
        f << id_ << ","
          << escapePath(path_) << ","
          << escapePath(githubUrl()) << std::endl;
    }

private:
    friend class Writer;
    friend class Crawler;
    friend class TokenizedFile;

    /** Called by worker when project is closed so that it can be deleted at the earliest convenience.
     */
    void close() {
        if (--files_ == 0)
            delete this;
    }

    unsigned id_ = 0;

    std::string path_;
    std::string url_;

    std::atomic_uint files_;
};

class TokenizedFile {
public:

    /** Returns absolute path of the file on local drive.
     */
    std::string absPath() {
        return project_->path_ + "/" + path_;
    }

    /** Outputs the sourcererCC's file statistics.

      These contain the project and file id, % escaped file path and file url on github, followed by file hash, number of bytes, lines, lines w/o empty lines and lines w/o empty or comment lines.
     */
    void sourcererCCFileStats(std::ostream & f) {
        f << project_->id_ << ","
          << id_ << ","
          << escapePath(STR(project_->path_ << "/" << path_)) << ","
          << escapePath(STR(project_->githubUrl() << "/blob/master/" << path_)) << ","
          << fileHash_ << ","
          << bytes_ << ","
          << loc_ << "," // size in lines
          << loc_ - emptyLoc_ << "," // LOC
          << loc_ - emptyLoc_ - commentLoc_ << std::endl; // SLOC
    }

    /** Outputs the file tokens in sourcererCC's format.

      This means project id, file id, # of total tokens, # of unique tokens, tokens hash and then `\` escaped tokens and their frequencies.
     */
    void sourcererCCFileTokens(std::ostream & f) {
        f << project_->id_ << ","
          << id_ << ","
          << totalTokens_ << ","
          << tokens_.size() << ","
          << tokensHash_ << "@#@";
        if (not tokens_.empty()) {
            auto i = tokens_.begin(), e = tokens_.end();
            f << escape(i->first) << "@@::@@" << i->second;
            ++i;
            while (i != e) {
                f << "," << escape(i->first) << "@@::@@" << i->second;
                ++i;
            }
        }
        f << std::endl;
    }

    /** Outputs all statistics gathered for the file.

      This is project and file ids, project path and file's relative path (% escaped), size in bytes, comments bytes, whitespace bytes, tokens bytes ans separator bytes, lines of code, comments only lines, empty lines, total tokens and file & tokens hash.
     */
    void ourData(std::ostream & f) {
        f << project_->id_ << ","
          << id_ << ","
          << escapePath(project_->path_) << ","
          << escapePath(path_) << ","
          << bytes_ << ","
          << commentBytes_ << ","
          << whitespaceBytes_ << ","
          << tokenBytes_ << ","
          << (bytes_ - commentBytes_ - whitespaceBytes_ - tokenBytes_) << "," // separator bytes
          << loc_ << ","
          << commentLoc_ << ","
          << emptyLoc_ << ","
          << totalTokens_ << ","
          << tokens_.size() << ","
          << fileHash_ << ","
          << tokensHash_ << std::endl;
    }


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
    friend class Tokenizer;

    /** Calculates hash of the unique tokens and their frequencies in the file.
     */
    void calculateTokensHash() {
        MD5 md5;
        for (auto i : tokens_) {
            md5.add(i.first.c_str(), i.first.size());
            md5.add(& i.second, sizeof(i.second));
        }
        tokensHash_ = md5.getHash();
    }

    unsigned id_ = 0;

    GithubProject * project_;
    std::string path_;

    unsigned long bytes_ = 0;
    unsigned long commentBytes_ = 0;
    unsigned long whitespaceBytes_ = 0;
    unsigned long tokenBytes_ = 0;

    unsigned loc_ = 0;
    unsigned commentLoc_ = 0;
    unsigned emptyLoc_ = 0;

    unsigned totalTokens_ = 0;

    std::string tokensHash_;
    std::string fileHash_;

    std::map<std::string, unsigned> tokens_;






};
