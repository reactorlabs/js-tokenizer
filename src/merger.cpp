#include <thread>
#include <iomanip>
#include "merger.h"
#include "writer.h"

unsigned Merger::fid_ = FILE_ID_STARTS_AT;
unsigned Merger::pid_ = PROJECT_ID_STARTS_AT;

Merger::StopClones Merger::stopClones_ = Merger::StopClones::tokens;
std::map<std::string, Merger::CloneInfo> Merger::clones_;

std::map<std::string, Merger::TokenInfo> Merger::uniqueTokenIds_;

unsigned Merger::numClones_ = 0;

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
          << escapeToken(i.first) << std::endl;
    }
}

Merger::CloneInfo Merger::checkClones(TokenizedFile * tf) {
    if (stopClones_ == StopClones::none)
        return CloneInfo();
    std::string const & hash = stopClones_ == StopClones::file ? tf->stats.fileHash() : tf->stats.tokensHash();
    auto i = clones_.find(hash);
    if (i == clones_.end()) {
        clones_[hash] = CloneInfo(tf->pid(), tf->id());
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
        auto j = uniqueTokenIds_.find(i.first);
        if (j == uniqueTokenIds_.end())
            j = uniqueTokenIds_.insert(std::pair<std::string, TokenInfo>(i.first, TokenInfo(uniqueTokenIds_.size()))).first;
        ++(j->second);
        tm.add(STR(std::hex << j->second.id), i.second);
    }
    tf->updateTokenMap(std::move(tm));
}

void Merger::process(MergerJob const & job) {
    bool writeProject = false;
    TokenizedFile * tf = job.file;
    tf->setId(fid_++);
    if (tf->pid() == 0) {
        tf->setPid(pid_++);
        writeProject = true;
    }

    CloneInfo ci = checkClones(tf);

    idsForTokens(tf);


    Writer::Schedule(WriterJob(job.file, writeProject, ci.pid, ci.fid));

    processedBytes_ += tf->stats.bytes();
    ++processedFiles_;
}
