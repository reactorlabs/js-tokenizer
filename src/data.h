#pragma once

#include <memory>
#include <map>
#include <unordered_map>
#include <atomic>
#include <condition_variable>

#include "helpers.h"
#include "worker.h"

template<typename T>
class ObjectsCounter {
public:

    static unsigned MaxInstances() {
        return maxInstances_;
    }

    static unsigned Instances() {
        return instances_;
    }

    static void SetMaxInstances(unsigned value) {
        maxInstances_ = value;
    }

    virtual ~ObjectsCounter() {
        std::lock_guard<std::mutex> g(m_);
        if (--instances_ < maxInstances_)
            cv_.notify_one();
    }

protected:

    ObjectsCounter() {
        std::unique_lock<std::mutex> g(m_);
        while (instances_ >= maxInstances_) {
            Thread::Stall();
            cv_.wait(g);
            Thread::Resume();
        }
        ++instances_;
    }

private:
    static unsigned maxInstances_;
    static unsigned instances_;

    static std::mutex m_;
    static std::condition_variable cv_;
};

template<typename T>
unsigned ObjectsCounter<T>::maxInstances_(10000);

template<typename T>
unsigned ObjectsCounter<T>::instances_(0);

template<typename T>
std::mutex ObjectsCounter<T>::m_;

template<typename T>
std::condition_variable ObjectsCounter<T>::cv_;



/** Identifies the tokenizer in case when multiple tokenizers are being used.
 */
enum class TokenizerKind {
    Generic,
    JavaScript
};

/** Returns the prefix used for tables and output files based on the tokenizer used.
 */
inline std::string prefix(TokenizerKind k) {
    switch (k) {
        case TokenizerKind::Generic:
            return "";
        case TokenizerKind::JavaScript:
            return "js_";
    }
}

/** Contains the information about a single cloned project.
 */

class ClonedProject : public ObjectsCounter<ClonedProject> {
public:

    ClonedProject(int id, std::string const & url, unsigned createdAt):
        id(id),
        url(url),
        createdAt(createdAt) {
    }

    /** Returns the url to be used to clone the project.
     */
    std::string cloneUrl() const {
        return STR("http://github.com/" << url << ".git");
    }

    int id;
    std::string url;
    unsigned createdAt;
    std::string path;
};

/** Contains a tokenized file.
 */
class TokenizedFile : public ObjectsCounter<TokenizedFile> {
public:

    TokenizedFile(std::shared_ptr<ClonedProject> const & project, std::string relPath, unsigned cdate):
        id(idCounter_++),
        project(project),
        relPath(relPath),
        createdAt(cdate) {
    }

    TokenizerKind tokenizer;
    int id = -1;
    std::shared_ptr<ClonedProject> project;
    std::string relPath;
    unsigned createdAt = 0;

    // Errors observed while tokenizing the file
    unsigned errors = 0;

    unsigned totalTokens = 0;
    unsigned uniqueTokens = 0;
    unsigned lines = 0;
    unsigned loc = 0;
    unsigned sloc = 0;
    unsigned bytes = 0;
    unsigned whitespaceBytes = 0;
    unsigned commentBytes = 0;
    unsigned separatorBytes = 0;
    unsigned tokenBytes = 0;
    Hash fileHash;
    Hash tokensHash;

    // Information about the clone group, if the file belongs to one, or -1
    int groupId = -1;

    // True if the file has unique file hash
    bool fileHashUnique = false;

private:

    static std::atomic_uint idCounter_;
};

class TokensMap : public ObjectsCounter<TokensMap> {
public:
    TokensMap(std::shared_ptr<TokenizedFile> file):
        file(file) {
    }

    unsigned & operator[] (std::string const & index) {
        return tokens[index];
    }

    std::shared_ptr<TokenizedFile> file;

    std::unordered_map<std::string, unsigned> tokens;
};

class CloneGroup {
public:
    int id;
    unsigned files;
    int oldestId;
    unsigned oldestTime;

    CloneGroup():
        id(-1),
        oldestId(-1),
        oldestTime(-1) {
    }

    int update(TokenizedFile & f) {
        if (id == -1) {
            id = f.id;
            oldestId = f.id;
            oldestTime = f.createdAt;
            return -1;
        } else {
            if (oldestTime > f.createdAt) {
                oldestId = f.id;
                oldestTime = f.createdAt;
            }
            return id;
        }
    }

};

class TokenInfo {
public:
    int id;
    unsigned uses;
    unsigned size;

    TokenInfo():
        id(idCounter_++),
        uses(0),
        size(0) {
    }

    bool update(std::string const & token, unsigned uses) {
        if (this->uses == 0) {
            this->uses = uses;
            this->size = token.size();
            return true;
        } else {
            this->uses += uses;
            return false;
        }
    }

private:
    static std::atomic_uint idCounter_;
};

/** New tokens from each file as passed to the DB writer.
 */
class NewTokens {
public:
    TokenizerKind tokenizer;
    std::map<int, std::string> tokens;

    NewTokens(TokenizerKind kind):
        tokenizer(kind) {
    }

    std::string & operator [] (int index) {
        return tokens[index];
    }
};

class MergerStats {
public:
    TokenizerKind tokenizer;
    std::map<Hash, CloneGroup> const & cloneGroups;
    std::map<Hash, TokenInfo> const & tokens;

    MergerStats(TokenizerKind kind, std::map<Hash, CloneGroup> const & cloneGroups, std::map<Hash, TokenInfo> const & tokens):
        tokenizer(kind),
        cloneGroups(cloneGroups),
        tokens(tokens) {
    }
};



