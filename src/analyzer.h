#pragma once

#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>

#include "utils.h"
#include "data.h"
#include "worker.h"


/** The job of analyzer is to perform some analysis & conversion on the acquired data.
 */
class Analyzer : public Worker {
public:
    void addFile(TokenizedFile* f) {

    }





private:
    static std::mutex queueAccess_;
    static std::queue<TokenizedFile *> q_;
    static std::condition_variable qCV_;

#if NUM_ANALYZERS > 1
    static std::mutex processingAccess_;
#endif

};
