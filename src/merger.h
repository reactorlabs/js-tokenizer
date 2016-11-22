#pragma once

#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "tokenizer.h"
#include "worker.h"
#include "DBWriter.h"
#include "data.h"

template<TokenizerType TYPE>
struct MergerJob {
    TokenizedFile * file;

    MergerJob(TokenizedFile * file):
        file(file) {
    }

    friend std::ostream & operator << (std::ostream & s, MergerJob const & job) {
        if (job.file != nullptr)
            s << job.file->path();
        else
            s << "Writing final statistics";
        return s;
    }
};


template<TokenizerType TYPE>
class Merger: public QueueWorker<MergerJob<TYPE> > {
public:

    Merger(unsigned index):
        QueueWorker<MergerJob<TYPE >>(STR("MERGER_" << TYPE), index) {
    }

private:

    void updateCloneInfo(TokenizedFile * f) {
        std::lock_guard<std::mutex> g(mCloneInfo_);
        cloneInfo_[f->tokensHash].updateWith(*f);
    }

    void checkUniqueFileHash(TokenizedFile * f) {
        std::lock_guard<std::mutex> g(mFileHash_);
        auto i = uniqueFileHashes_.find(f->fileHash);
        if (i == uniqueFileHashes_.end()) {
            f->fileHashUnique = true;
            uniqueFileHashes_.insert(f->fileHash);
        }
    }

    void updateTokenInfo(TokenizedFile * f) {
        std::unordered_map<std::string, unsigned> translatedTokens;
        std::map<int, std::string> newTokens;
        for (auto & i : f->tokens) {
            // get the hash
            hash h = i.first;
            std::pair<unsigned, bool> t;
            {
                std::lock_guard<std::mutex> g(mTokens_);
                TokenInfo & ti = tokens_[h];
                if (ti.id == -1)
                    ti.id = tokens_.size() - 1;
                t = ti.updateWith(i.first, i.second);
            }
            // if the token was new, schedule writing the new token to the database
            if (t.second)
                newTokens[t.first] = i.first;
            // add the token to the translated tokens
            translatedTokens[STR(std::hex << t.first)] = i.second;
        }
        // replace the tokens with translated tokens;
        f->tokens = std::move(translatedTokens);
        if (not newTokens.empty())
            Writer<TYPE>::Schedule(WriterJob<TYPE>::NewToken(newTokens), this);

    }

    void process(MergerJob<TYPE> const & job) override {
        if (job.file == nullptr) {
            if (not tokens_.empty()) {
                Worker::Print(STR("Writing token statistics for " << tokens_.size() << " tokens"));
                Writer<TYPE>::Schedule(WriterJob<TYPE>::TokenInfo(&tokens_), this);
            }
            if (not cloneInfo_.empty()) {
                Worker::Print(STR("Writing clone group statistics for " << cloneInfo_.size() << " groups"));
                    Writer<TYPE>::Schedule(WriterJob<TYPE>::CloneGroups(&cloneInfo_), nullptr);
            }
        } else {
            updateCloneInfo(job.file);
            checkUniqueFileHash(job.file);
            updateTokenInfo(job.file);
            // now launch the writer
            Writer<TYPE>::Schedule(WriterJob<TYPE>::File(job.file), this);
        }
    }

    // clone groups
    static std::unordered_map<hash, CloneInfo> cloneInfo_;

    // token information
    static std::unordered_map<hash, TokenInfo> tokens_;

    static std::unordered_set<hash> uniqueFileHashes_;

    static std::mutex mCloneInfo_;
    static std::mutex mFileHash_;
    static std::mutex mTokens_;
};




template<TokenizerType TYPE>
std::unordered_map<hash, CloneInfo> Merger<TYPE>::cloneInfo_;

template<TokenizerType TYPE>
std::unordered_map<hash, TokenInfo> Merger<TYPE>::tokens_;

template<TokenizerType TYPE>
std::unordered_set<hash> Merger<TYPE>::uniqueFileHashes_;

template<TokenizerType TYPE>
std::mutex Merger<TYPE>::mCloneInfo_;
template<TokenizerType TYPE>
std::mutex Merger<TYPE>::mFileHash_;
template<TokenizerType TYPE>
std::mutex Merger<TYPE>::mTokens_;
