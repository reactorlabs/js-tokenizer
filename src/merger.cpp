#include <thread>
#include <iomanip>
#include "merger.h"
#include "writer.h"

std::atomic_uint Merger::fid_(FILE_ID_STARTS_AT);
std::atomic_uint Merger::pid_(PROJECT_ID_STARTS_AT);

Merger::StopClones Merger::stopClones_ = Merger::StopClones::tokens;
std::unordered_map<std::string, Merger::CloneInfo> Merger::clones_;
std::unordered_map<std::string, Merger::TokenInfo> Merger::uniqueTokenIds_;

std::mutex Merger::accessM_;

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
    for (auto i : uniqueTokenIds_) {
        s << i.second.id << ","
          << i.second.count << ","
          << i.first.size() << ","
          << escapeToken(i.first) << std::endl;
    }
}

Merger::CloneInfo Merger::checkClones(TokenizedFile * tf) {
    if (stopClones_ == StopClones::none)
        return CloneInfo();
    std::string const & hash = stopClones_ == StopClones::file ? tf->stats.fileHash() : tf->stats.tokensHash();
    accessM_.lock();
    auto i = clones_.find(hash);
    accessM_.unlock();
    if (i == clones_.end()) {
        accessM_.lock();
        clones_[hash] = CloneInfo(tf->pid(), tf->id());
        accessM_.unlock();
        return CloneInfo();
    } else {
        ++numClones_;
        Writer::Log("clone");
        return i->second;
    }
}

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
}

void Merger::process(MergerJob const & job) {
    bool writeProject = false;
    TokenizedFile * tf = job.file;
    tf->setId(fid_++);
    accessM_.lock();
    if (tf->pid() == 0) {
        tf->setPid(pid_++);
        writeProject = true;
    }
    accessM_.unlock();

    idsForTokens(tf);

    tf->calculateTokensHash();


    CloneInfo ci = checkClones(tf);



    processedBytes_ += tf->stats.bytes();
    ++processedFiles_;
    if (tf->stats.errors > 0)
        ++numErrorFiles_;
    if (tf->stats.totalTokens == 0)
        ++numEmptyFiles_;

    Writer::Schedule(WriterJob(job.file, writeProject, ci.pid, ci.fid));

}
