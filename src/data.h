#pragma once
#include <set>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <atomic>

#include "utils.h"
#include "config.h"

#ifdef HAHA


constexpr unsigned FILE_ID_STARTS_AT = 1;
constexpr unsigned PROJECT_ID_STARTS_AT = 1;


class ClonedProject;


/** Token map.

  Contains a map of tokens and their frequencies.
 */
class TokenMap {
public:
    typedef std::map<std::string, unsigned>::const_iterator const_iterator;
    typedef std::map<std::string, unsigned>::iterator iterator;

    void clear() {
        freqs_.clear();
    }

    const_iterator begin() const {
        return freqs_.begin();
    }

    const_iterator end() const {
        return freqs_.end();
    }

    void add(std::string const & token) {
        ++freqs_[token];
    }

    void add(std::string const & token, unsigned freq) {
        freqs_[token] += freq;
    }

    /** Outputs the tokens and their frequencies in the sourcererCC's format.
     */
    void writeSourcererFormat(std::ostream & s);

    unsigned size() const {
        return freqs_.size();
    }

private:
    friend class TokenizedFile;


    std::string calculateHash();

    std::map<std::string, unsigned> freqs_;
};


class FileStats {
public:
    static int objects(int increment = 0) {
        static unsigned objects = 0;
        objects += increment;
        return objects;
    }

    static void parseFile(std::string const & filename);

    static FileStats * get(size_t id) {
        id -= FILE_ID_STARTS_AT;
        assert(id < files_.size());
        return files_[id];
    }

    static size_t NumFiles() {
        return files_.size();
    }

    unsigned bytes() const {
        return bytes_;
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

    std::string const & tokensHash() const {
        return tokensHash_;
    }

    /** Writes the file statistics into given stream.

      File statistics are written in the format our analysis tools (including the tokenizer's analyzer and validator) expect it, and *not* in the sourcererCC's format.
     */
    void writeFullStats(std::ostream & s);

    /** Writes sourcererCC's statistics into given stream.

      These are used by the sourcererCC's tools and analyzer and validator ignore them. Only non-empty (token-wise) files should be are reported.
     */
    void writeSourcererStats(std::ostream & s);


    unsigned id_ = 0;


    unsigned totalTokens = 0;
    unsigned uniqueTokens_ = 0;
    unsigned errors = 0;

    unsigned createdDate;

    unsigned loc_ = 1;
    unsigned commentLoc_ = 0;
    unsigned emptyLoc_ = 0;
    unsigned tokenBytes_ = 0;

private:
    friend class TokenizedFile;

    FileStats() = default;

    FileStats(GitProject * project, std::string const & relPath):
        project_(project),
        relPath_(relPath) {
    }

    void loadFrom(std::string const & tmp);

    GitProject * project_ = nullptr;
    std::string relPath_;
    unsigned bytes_ = 0;
    unsigned commentBytes_ = 0;
    unsigned whitespaceBytes_ = 0;
    unsigned separatorBytes_ = 0;




    std::string fileHash_;
    std::string tokensHash_;

    static std::vector<FileStats *> files_;
};

class TokenizedFile2 {
public:

    ClonedProject * project;
    unsigned id;
    unsigned totalTokens;
    unsigned uniqueTokens;
    unsigned errors;
    unsigned createdDate;
    unsigned loc;
    unsigned commentloc;
    unsigned emptyLoc;
    unsigned bytes;
    unsigned whitespaceBytes;
    unsigned commentBytes;
    unsigned separatorBytes;
    unsigned tokenBytes;
    std::string fileHash;
    std::string tokensHash;

    std::map<std::string, unsigned> tokens;


    bool empty() const {
        return tokens.empty();
    }



    void updateFileStats(std::string const & contents);

    void calculateTokensHash() {
        stats.tokensHash_ = tokens.calculateHash();
    }

    void addToken(std::string const & token) {
        ++stats.totalTokens;
        stats.tokenBytes_ += token.size();
        tokens.add(token);
    }

    void updateTokenMap(TokenMap && map) {
        tokens.freqs_ = std::move(map.freqs_);
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
        ++stats.errors;
    }

    std::string absPath() {
        return stats.absPath();
    }

    unsigned id() const {
        return stats.id_;
    }

    unsigned pid() const {
        return stats.project_->id_;
    }

    ClonedProject * project() const {
        return stats.project_;
    }

    void setId(unsigned id) {
        assert(stats.id_ == 0);
        stats.id_ = id;
    }

    void setPid(unsigned id) {
        assert(stats.project_->id_ == 0);
        stats.project_->id_ = id;
    }

    /** Outputs the tokens in sourcererCC's format.
     */
    void writeTokens(std::ostream & s);

    TokenizedFile(ClonedProject * project, std::string const & relPath):
        stats(project, relPath) {
        ++project->handles_;
    }

    TokenizedFile() = default;


    /** Deletes the tokenized file.

      If it is the last handle to the project class, deletes the project class as well.
     */
    ~TokenizedFile() {
        if (stats.project_ != nullptr)
            if (--stats.project_->handles_ == 0)
                delete stats.project_;
    }

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

    void writeTo(std::ostream & s) {
        s << pid1_ << "," << fid1_ << "," << pid2_ << "," << fid2_ << std::endl;
    }

    CloneInfo(unsigned pid1, unsigned fid1, unsigned pid2, unsigned fid2):
        pid1_(pid1),
        fid1_(fid1),
        pid2_(pid2),
        fid2_(fid2) {
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
    static std::unordered_map<unsigned, CloneGroup *> idToGroup_;

};



#endif
