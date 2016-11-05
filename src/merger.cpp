#include <thread>
#include <iomanip>
#include "merger.h"
#include "writer.h"


/** Possible merger speedups:


 - move pid assignment to writer

 - since tokenizer is linear now, only first file writes the project

 - pay price only when adding new token






 */

Merger::StopClones Merger::stopClones_ = Merger::StopClones::tokens;
std::unordered_map<std::string, Merger::CloneInfo> Merger::clones_;
//std::unordered_map<std::string, Merger::TokenInfo> Merger::uniqueTokenIds_;


std::unordered_map<std::string, unsigned> Merger::tokenIds_;
std::vector<unsigned> Merger::tokenCounts_;



std::mutex Merger::accessC_;
std::mutex Merger::accessTid_;
std::mutex Merger::accessTc_;

std::atomic_uint Merger::numClones_(0);
std::atomic_uint Merger::numEmptyFiles_(0);
std::atomic_uint Merger::numErrorFiles_(0);

void Merger::initializeWorkers(unsigned num) {
    for (unsigned i = 0; i < num; ++i) {
        std::thread t([i] () {
            Merger c(i);
            c();
        });
        t.detach();
    }
}

void Merger::writeGlobalTokens(std::ostream & s) {
    for (auto i : tokenIds_) {
        s << i.second << ","
          << tokenCounts_[i.second] << ","
          << i.first.size() << ","
          << escapeToken(i.first) << std::endl;
    }
}

Merger::CloneInfo Merger::checkClones(TokenizedFile * tf) {
    if (stopClones_ == StopClones::none)
        return CloneInfo();
    std::string const & hash = stopClones_ == StopClones::file ? tf->stats.fileHash() : tf->stats.tokensHash();
    accessC_.lock();
    auto i = clones_.find(hash);
    if (i == clones_.end()) {
        clones_[hash] = CloneInfo(tf->pid(), tf->id());
        accessC_.unlock();
        return CloneInfo();
    } else {
        accessC_.unlock();
        ++numClones_;
        Writer::Log("clone");
        return i->second;
    }
}
/*
void Merger::idsForTokens(TokenizedFile * tf) {
    TokenMap tm;
    for (auto i : tf->tokens) {
        accessM_.lock();
        auto j = uniqueTokenIds_.find(i.first);
        if (j == uniqueTokenIds_.end())
            j = uniqueTokenIds_.insert(std::pair<std::string, TokenInfo>(i.first, TokenInfo(uniqueTokenIds_.size()))).first;
        ++(j->second);
        accessM_.unlock();
        tm.add(STR(std::hex << j->second.id), i.second);
    }
    tf->updateTokenMap(std::move(tm));
} */

// TODO these are dumb for now
void Merger::lockTokenIdRead() {
    accessTid_.lock();
}

void Merger::unlockTokenIdRead() {
    accessTid_.unlock();
}

void Merger::lockTokenIdWrite() {
    accessTid_.lock();
}

void Merger::unlockTokenIdWrite() {
    accessTid_.unlock();
}

void Merger::lockCounts() {
    accessTc_.lock();
}

void Merger::unlockCounts() {
    accessTc_.unlock();
}

void Merger::tokensToIds(TokenizedFile * tf) {
    std::map<unsigned, unsigned> matched;
    TokenMap missing;

    // first get id's of tokens that are definitely in the map already
    // many threads can do this at the same time
    lockTokenIdRead();
    for (auto i : tf->tokens) {
        auto j = tokenIds_.find(i.first);
        if (j == tokenIds_.end())
            missing.add(i.first, i.second);
        else
            matched[j->second] = i.second;
    }
    unlockTokenIdRead();

    // now obtain id's for those that were not in the map the first time, note someone might have added them in the meantime
    // only one thread can do this at a time
    lockTokenIdWrite();
    for (auto i : missing) {
        auto j = tokenIds_.find(i.first);
        if (j == tokenIds_.end()) {
            unsigned id = tokenIds_.size();
            tokenIds_[i.first] = id;
            matched[id] = i.second;
        } else {
            matched[j->second] = i.second;
        }
    }
    unlockTokenIdWrite();

    // now get the largest id we have
    unsigned maxId = 0;
    for (auto i : matched) {
        if (i.first > maxId)
            maxId = i.first;
    }

    // update token counts
    // only one thread can do this at a time
    lockCounts();
    // resize if needed
    while (maxId >= tokenCounts_.size())
        tokenCounts_.resize(tokenCounts_.size() * 2);
    // update the counts
    for (auto i : matched)
        tokenCounts_[i.first] += i.second;
    unlockCounts();

    // convert the token id's and counts back into token map
    tf->tokens.clear();
    for (auto i : matched)
        tf->tokens.add(STR(std::hex << i.first), i.second);
}



void Merger::process(MergerJob const & job) {
    TokenizedFile * tf = job.file;

    tokensToIds(tf);

    // calculate hash for the tokens and determine if the file is a clone of someone
    tf->calculateTokensHash();
    CloneInfo ci = checkClones(tf);

    // update statistics
    processedBytes_ += tf->stats.bytes();
    ++processedFiles_;
    if (tf->stats.errors > 0)
        ++numErrorFiles_;
    if (tf->stats.totalTokens == 0)
        ++numEmptyFiles_;

    // schedule writing of the file
    Writer::Schedule(WriterJob(job.file, ci.pid, ci.fid));
}
