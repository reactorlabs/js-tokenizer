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

inline std::ostream & operator << (std::ostream & s, TokenizerKind t) {
    switch (t) {
        case TokenizerKind::Generic:
            s << "generic";
            break;
        case TokenizerKind::JavaScript:
            s << "js";
            break;
    }
    return s;
}

/** Contains the information about a single cloned project.
 */

class ClonedProject : public ObjectsCounter<ClonedProject> {
public:

    static unsigned & StrideCount() {
        return stride_count_;
    }

    static unsigned & StrideIndex() {
        return stride_index_;
    }

    static bool & KeepProjects() {
        return keepProjects_;
    }

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

    /** Deletes the project from disk.
     */
    void deleteFromDisk() {
        if (system(STR("rm -rf " << path).c_str()) != EXIT_SUCCESS)
            Thread::Log(STR("Unable to delete folder " << path));
    }

    ~ClonedProject() {
        if (not keepProjects_)
            deleteFromDisk();
    }

    int id;
    std::string url;
    unsigned createdAt;
    std::string path;
    std::string commit;

private:
    static bool keepProjects_;
    /** Number of strides to be executed */
    static unsigned stride_count_;
    /** Index of the current stride */
    static unsigned stride_index_;
};

/** Contains a tokenized file.
 */
class TokenizedFile : public ObjectsCounter<TokenizedFile> {
public:

    static void InitializeStrideId() {
        idCounter_ = ClonedProject::StrideIndex();
    }

    /** Retrns new id for a file so that the same files tokenized by different tokenizers have the same ids.
     */
    static int GetNewId() {
        return idCounter_.fetch_add(ClonedProject::StrideCount());
    }

    TokenizedFile(int id, std::shared_ptr<ClonedProject> const & project, std::string relPath, unsigned cdate):
        id(id),
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
    int oldestId;
    unsigned oldestTime;

    CloneGroup():
        id(-1),
        oldestId(-1),
        oldestTime(-1) {
    }

    CloneGroup(int id, int oldestId, unsigned oldestTime):
        id(id),
        oldestId(oldestId),
        oldestTime(oldestTime) {
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

    TokenInfo():
        id(idCounter_++),
        uses(0) {
    }

    TokenInfo(int id, unsigned uses):
        id(id),
        uses(uses) {
        idCounter_ = id + 1; // update the id counter
    }

    bool update(std::string const & token, unsigned uses) {
        if (this->uses == 0) {
            this->uses = uses;
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
    enum class Kind {
        CloneGroup,
        TokenInfo
    };

    TokenizerKind tokenizer;

    Kind kind;

    union {
        CloneGroup const * cloneGroup;
        TokenInfo const * tokenInfo;
    };

    MergerStats(TokenizerKind kind, CloneGroup const * group):
        tokenizer(kind),
        kind(Kind::CloneGroup),
        cloneGroup(group) {
    }

    MergerStats(TokenizerKind kind, TokenInfo const * info):
        tokenizer(kind),
        kind(Kind::TokenInfo),
        tokenInfo(info) {
    }
};

