#pragma once
#include <set>
#include <map>
#include <string>
#include <vector>
#include <iostream>

#include "utils.h"
#include "config.h"


/** Representation of a git project.

  Contains project id, path and url.
 */
class GitProject {
public:
    static void parseFile(std::string const & filename);

    std::string const & path() const {
        return path_;
    }

    std::string const & githubUrl() const {
        if (githubUrl_.empty())
            githubUrl_ = githubUrl(url_);
        return githubUrl_;
    }

    GitProject():
        id_(0) {
    }

    GitProject(std::string const & path, std::string const & url):
        id_(0),
        path_(path),
        url_(url) {
    }

private:
    friend class FileStats;
    friend class TokenizedFile;

    static std::string githubUrl(std::string const & giturl) {
        std::string url = giturl;
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
            return giturl; // not a github project, return project url
        }
        return "https://github.com/" + url;
    }

    void loadFrom(std::string const & tmp);

    unsigned id_;
    std::string path_;
    std::string url_;
    mutable std::string githubUrl_;

    static std::vector<GitProject *> projects_;

};


/** Token map.

  Contains a map of tokens and their frequencies.
 */
class TokenMap {
public:
    typedef std::map<std::string, unsigned>::const_iterator const_iterator;
    typedef std::map<std::string, unsigned>::iterator iterator;

    const_iterator begin() const {
        return freqs_.begin();
    }

    const_iterator end() const {
        return freqs_.end();
    }

    /** Outputs the tokens and their frequencies in the sourcererCC's format.
     */
    void writeSourcererFormat(std::ostream & s);


private:
    friend class TokenizedFile;

    void add(std::string const & token) {
        ++freqs_[token];
    }

    std::string calculateHash();

    std::map<std::string, unsigned> freqs_;
};


class FileStats {
public:
    static void parseFile(std::string const & filename);

    static FileStats * get(size_t id) {
        id -= FILE_ID_STARTS_AT;
        assert(id < files_.size());
        return files_[id];
    }

    static size_t numFiles() {
        return files_.size();
    }

    std::string absPath() const {
        return project_->path() + "/" + relPath_;
    }

    std::string githubUrl() const {
        return project_->githubUrl() + "/blob/master/" + relPath_;
    }

    std::string const & fileHash() const {
        return fileHash_;
    }

    /** Writes the file statistics into given stream.

      File statistics are written in the format our analysis tools (including the tokenizer's analyzer and validator) expect it, and *not* in the sourcererCC's format.
     */
    void writeFullStats(std::ostream & s);

    /** Writes sourcererCC's statistics into given stream.

      These are used by the sourcererCC's tools and analyzer and validator ignore them. Only non-empty (token-wise) files should be are reported.
     */
    void writeSourcererStats(std::ostream & s);

private:
    friend class TokenizedFile;

    FileStats() = default;

    FileStats(GitProject * project, std::string const & relPath):
        project_(project),
        relPath_(relPath) {
    }

    void loadFrom(std::string const & tmp);

    unsigned id_;
    GitProject * project_;
    std::string relPath_;
    unsigned bytes_ = 0;
    unsigned commentBytes_ = 0;
    unsigned whitespaceBytes_ = 0;
    unsigned tokenBytes_ = 0;
    unsigned separatorBytes_ = 0;

    unsigned loc_ = 0;
    unsigned commentLoc_ = 0;
    unsigned emptyLoc_ = 0;

    unsigned totalTokens_ = 0;
    unsigned uniqueTokens_ = 0;

    unsigned errors_ = 0;

    std::string fileHash_;
    std::string tokensHash_;

    static std::vector<FileStats *> files_;
};

class TokenizedFile {
public:

    TokenizedFile(GitProject * project, std::string const & relPath):
        stats(project, relPath) {
    }

    bool empty() const {
        return tokens.freqs_.empty();
    }

    void updateFileStats(std::string const & contents);

    void calculateTokensHash() {
        stats.tokensHash_ = tokens.calculateHash();
    }

    void addToken(std::string const & token) {
        ++stats.uniqueTokens_;
        stats.tokenBytes_ += token.size();
        tokens.add(token);
    }

    void addSeparator(size_t size) {
        stats.separatorBytes_ += size;
    }

    void addComment(size_t size) {
        stats.commentBytes_ += size;
    }

    void addWhitespace(size_t size) {
        stats.whitespaceBytes_ += size;
    }

    void newline(bool commentOnly, bool empty) {
        ++stats.loc_;
        if (commentOnly)
            if (empty)
                ++stats.emptyLoc_;
            else
                ++stats.commentLoc_;
    }

    void tokenizationError() {
        ++stats.errors_;
    }

    std::string absPath() {
        return stats.absPath();
    }

    /** Outputs the tokens in sourcererCC's format.
     */
    void writeTokens(std::ostream & s);



    FileStats stats;
    TokenMap tokens;
};

class CloneInfo {
public:
    static void parseFile(std::string const & filename);

    static CloneInfo * get(size_t index) {
        assert(index < clones_.size());
        return clones_[index];
    }

    static size_t numClones() {
        return clones_.size();
    }

    unsigned fid1() const {
        return fid1_;
    }

    unsigned fid2() const {
        return fid2_;
    }


private:
    CloneInfo() = default;


    void loadFrom(std::string const & tmp);

    unsigned pid1_;
    unsigned fid1_;
    unsigned pid2_;
    unsigned fid2_;

    static std::vector<CloneInfo *> clones_;
};

class CloneGroup {
public:
    /** Finds clone groups from the clone info parsed.
     */
    static void find();

    static CloneGroup * get(size_t index) {
        assert(index < groups_.size());
        return groups_[index];
    }

    static CloneGroup * getFor(unsigned id) {
        auto i = idToGroup_.find(id);
        if (i == idToGroup_.end())
            return nullptr;
        return i->second;
    }

    static size_t numGroups() {
        return groups_.size();
    }

    /** Writes the clone group into a stream.

      Clone group consists of its indices separated by commas.
     */
    void writeTo(std::ostream & s);

    /** Writes the clone group statistics
     */
    void writeStats(std::ostream & s);


private:

    std::set<unsigned> ids_;

    static std::vector<CloneGroup *> groups_;
    static std::map<unsigned, CloneGroup *> idToGroup_;

};



