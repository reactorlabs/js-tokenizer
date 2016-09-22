#pragma once

#include <dirent.h>

#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>

#include "data.h"

/** Writer, amongst other things writes the output.

  Writer is also responsible for keeping the global statistics.
 */
class Writer {
public:
    static void addTokenizedFile(TokenizedFile * f) {
        std::lock_guard<std::mutex> guard(queueAccess_);
        q_.push(f);
        qCV_.notify_one();
    }


private:

    static TokenizedFile * getSomethingToWrite() {
        std::unique_lock<std::mutex> guard(queueAccess_);
        if (q_.empty())
            qCV_.wait(guard);
        TokenizedFile * result = q_.front();
        q_.pop();
        return result;
    }

    /** Processes given file.
     */
    void processFile(TokenizedFile * f) {
        // if the project has not been processed, process it first
        if (f->project_->id_ == 0)
            processProject(f->project_);
        // assign file id
        f->id_ = ++numFiles_;
        // process statistics
        totalBytes_ += f->bytes_;

        // output file tokens and statistics
        // TODO output file stats & tokens

        // and finally, delete the file, if it is the last file in the project, deletes the project as well
        delete f;
    }

    void processProject(GithubProject * p) {
        p->id_ = ++numProjects_;
        // TODO output project stats in bookkeeping for sourcererCC

    }

    static std::mutex queueAccess_;
    static std::queue<TokenizedFile *> q_;
    static std::condition_variable qCV_;


    // hashmaps for checking whether file is exact hash or not
    std::map<std::string, unsigned> tokenHashMatches_;
    std::map<std::string, unsigned> fileHashMatches_;

    volatile unsigned long numFiles_;
    volatile unsigned long numProjects_;

    volatile unsigned long totalBytes_;
};
