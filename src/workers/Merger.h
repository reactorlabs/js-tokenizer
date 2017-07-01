#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "../data.h"
#include "../worker.h"

#include "Writer.h"

class MergerJob : public std::shared_ptr<TokensMap> {
public:
    MergerJob() {
    }

    MergerJob(std::shared_ptr<TokensMap> ptr):
        std::shared_ptr<TokensMap>(ptr) {
    }

    friend std::ostream & operator << (std::ostream & s, MergerJob const & j) {
        s << "File id " << j->file->id << ", url: " << j->file->project->url << "/" << j->file->relPath;
        return s;
    }
};



/*



 */
class Merger : public Worker<MergerJob> {
public:
    static char const * Name() {
        return "MERGER";
    }

    Merger(unsigned index):
        Worker<MergerJob>(Name(), index) {
    }

    static void AddTokenizer(TokenizerKind k) {
        unsigned idx = static_cast<unsigned>(k);
        if (contexts_.size() < idx + 1)
            contexts_.resize(idx + 1,nullptr);
        contexts_[idx] = new Context(k);
    }

    static unsigned NumCloneGroups() {
        unsigned result = 0;
        for (Context const * c : contexts_)
            if (c != nullptr)
                result += c->cloneGroups.size();
        return result;
    }

    static unsigned NumCloneGroups(TokenizerKind kind) {
        unsigned idx = static_cast<unsigned>(kind);
        if (idx >= contexts_.size())
            return 0;
        if (contexts_[idx] == nullptr)
            return 0;
        return contexts_[idx]->cloneGroups.size();
    }

    static unsigned NumTokens() {
        unsigned result = 0;
        for (Context const * c : contexts_)
            if (c != nullptr)
                result += c->tokenInfo.size();
        return result;
    }
    static unsigned NumTokens(TokenizerKind kind) {
        unsigned idx = static_cast<unsigned>(kind);
        if (idx >= contexts_.size())
            return 0;
        if (contexts_[idx] == nullptr)
            return 0;
        return contexts_[idx]->tokenInfo.size();
    }

    static unsigned UniqueFileHashes() {
        unsigned result = 0;
        for (Context const * c : contexts_)
            if (c != nullptr)
                result += c->uniqueFileHashes.size();
        return result;
    }

    static unsigned UniqueFileHashes(TokenizerKind kind) {
        return contexts_[static_cast<unsigned>(kind)]->uniqueFileHashes.size();
    }

    static unsigned UniqueTokenHashes(TokenizerKind kind) {
        return contexts_[static_cast<unsigned>(kind)]->uniqueTokenHashes;
    }



    static void AddUniqueFileHash(TokenizerKind kind, Hash hash) {
        contexts_[static_cast<unsigned>(kind)]->uniqueFileHashes.insert(hash);
    }

    static void AddCloneGroup(TokenizerKind kind, Hash hash, CloneGroup const & group) {
        contexts_[static_cast<unsigned>(kind)]->cloneGroups[hash] = group;
    }

    static void AddTokenInfo(TokenizerKind kind, Hash hash, TokenInfo const & group) {
        contexts_[static_cast<unsigned>(kind)]->tokenInfo[hash] = group;
    }

    static void FlushStatistics() {
        for (unsigned i = 0; i < contexts_.size(); ++i) {
            Context * c = contexts_[i];
            if (c == nullptr)
                continue;
            Buffer & cloneGroups = Buffer::Get(Buffer::Kind::CloneGroups, c->tokenizer);
            Buffer & clonePairs = Buffer::Get(Buffer::Kind::ClonePairs, c->tokenizer);
            Buffer & tokens = Buffer::Get(Buffer::Kind::Tokens, c->tokenizer);

            for (auto const & ii : c->cloneGroups) {
                cloneGroups.append(STR(
                    ii.second.id << "," <<
                    ii.second.oldestId));
                clonePairs.append(STR(
                    ii.second.id << "," <<
                    ii.second.id));
            }
            for (auto const & ii : c->tokenInfo) {
                tokens.append(STR(
                    ii.second.id << "," <<
                    ii.second.uses));
            }
        }
    }

private:
    struct Context {
        TokenizerKind tokenizer;
        std::unordered_map<Hash, CloneGroup> cloneGroups;
        std::unordered_map<Hash, TokenInfo> tokenInfo;
        // TODO this is wasteful, we just need hash -> 2 booleans
        std::unordered_set<Hash> uniqueFileHashes;
        std::mutex mCloneGroups;
        std::mutex mTokenInfo;
        std::mutex mFileHash;
        // num of files with unique token hashes (i.e. those that would go to sourcererCC)
        std::atomic_uint uniqueTokenHashes;


        Context(TokenizerKind k):
            tokenizer(k) {
        }
    };

    friend class Context;

    bool hasUniqueFileHash(TokenizedFile & tf, Context & context) {
        std::lock_guard<std::mutex> g(context.mFileHash);
        return context.uniqueFileHashes.insert(tf.fileHash).second;
    }

    /** Sets the clone group information for the file.
     */
    int getCloneGroup(Context & context) {
       std::lock_guard<std::mutex> g(context.mCloneGroups);
       return context.cloneGroups[job_->file->tokensHash].update(* job_->file);
    }

    /** Translates tokens to ids unique across the entire files.

      Schedules any newly created tokens to be written to the database at the end.
     */
    void translateTokensToIds(Context & context) {
        Buffer & b = Buffer::Get(Buffer::Kind::TokensText, context.tokenizer);
        std::unordered_map<std::string, unsigned> translatedTokens;
        for (auto & i : job_->tokens) {
            // create a hash of the token
            Hash h = i.first;
            unsigned id = -1;
            bool isNew = false;
            {
                std::lock_guard<std::mutex> g(context.mTokenInfo);
                TokenInfo &ti = context.tokenInfo[h];
                id = ti.id;
                isNew = ti.update(i.first, i.second);
            }
            // check if the token is new
            if (isNew) {
                // if the token is too large, i.e. more than 10kb show only first and last 1k characters
                if (i.first.size() > 10 * 1024) {
                    b.append(STR(
                         id << "," <<
                         i.first.size() << "," <<
                         escape(h) << "," <<
                         escape(STR(i.first.substr(0, 1000) << "......" << i.first.substr(i.first.size() - 1000)))));

                } else {
                    b.append(STR(
                         id << "," <<
                         i.first.size() << "," <<
                         escape(h) << "," <<
                         escape(i.first)));
                }
            }
            // create a record for the translated token and its uses
            translatedTokens[STR(std::hex << id)] = i.second;
        }
        // swap translated tokens for the original tokens in the token map
        job_->tokens = std::move(translatedTokens);
    }

    void process(Context & c) {
        // if the file has unique hash output also its statistics
        if (hasUniqueFileHash(* job_->file, c)) {
            Buffer::Get(Buffer::Kind::Stats, c.tokenizer).append(STR(
                escape(job_->file->fileHash) << "," <<
                job_->file->bytes << "," <<
                job_->file->lines << "," <<
                job_->file->loc << "," <<
                job_->file->sloc << "," <<
                job_->file->totalTokens << "," <<
                job_->file->uniqueTokens << "," <<
                escape(job_->file->tokensHash)));
        }
        // now let's convert tokens to their unique files
        translateTokensToIds(c);
        // get the group id and output a clone pair if not -1
        int groupId = getCloneGroup(c);
        if (groupId == -1) {
            // increase number of unique token hashes
            ++c.uniqueTokenHashes;
            if (not job_->tokens.empty()) {
                std::stringstream ss;
                ss << job_->file->project->id << "," <<
                     job_->file->id << "," <<
                     job_->file->totalTokens << "," <<
                     job_->file->uniqueTokens << "," <<
                     escape(job_->file->tokensHash) << "," <<
                     "@#@";
                auto i = job_->tokens.begin(), e = job_->tokens.end();
                ss << i->first << "@@::@@" << i->second;
                while (++i != e)
                    ss << "," << i->first << "@@::@@" << i->second;
                Buffer::Get(Buffer::Kind::TokenizedFiles, c.tokenizer).append(ss.str());
            }
        } else {
            // emit the clone pair
            Buffer::Get(Buffer::Kind::ClonePairs, c.tokenizer).append(STR(
                job_->file->id << "," <<
                groupId));
        }
    }

    virtual void process() {
        // get context for the used tokenizer and process tokens & clone pair information
        process(*contexts_[static_cast<unsigned>(job_->file->tokenizer)]);
    }

    static std::vector<Context * > contexts_;

    //static std::unordered_map<Buffer::ID, Buffer *> buffers_;

};
