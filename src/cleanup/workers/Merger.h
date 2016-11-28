#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "../data.h"
#include "../worker.h"

#include "DBWriter.h"
#include "Writer.h"

class MergerJob : public std::shared_ptr<TokensMap> {
public:
    MergerJob() {
    }

    MergerJob(std::shared_ptr<TokensMap> ptr):
        std::shared_ptr<TokensMap>(ptr) {
    }
};



/*



 */
class Merger : public Worker<MergerJob> {
public:
    Merger(unsigned index):
        Worker<MergerJob>("MERGER", index) {
    }

    static void AddTokenizer(TokenizerKind k) {
        unsigned idx = static_cast<unsigned>(k);
        if (contexts_.size() < idx + 1)
            contexts_.resize(idx + 1);
        contexts_[idx] = new Context();
    }

private:
    struct Context {
        std::unordered_map<Hash, CloneGroup> cloneGroups;
        std::unordered_map<Hash, TokenInfo> tokenInfo;
        std::mutex mCloneGroups;
        std::mutex mTokenInfo;

    };

    bool hasUniqueFileHash(TokenizedFile & tf) {
        std::lock_guard<std::mutex> g(mFileHash_);
        return uniqueFileHashes_.insert(tf.fileHash).second;
    }

    /** Sets the clone group information for the file.
     */
    void getCloneGroup(Context & context) {
       std::lock_guard<std::mutex> g(context.mCloneGroups);
       job_->file->groupId = context.cloneGroups[job_->file->tokensHash].update(* job_->file);
    }

    /** Translates tokens to ids unique across the entire files.

      Schedules any newly created tokens to be written to the database at the end.
     */
    void translateTokensToIds(Context & context) {
        std::unordered_map<std::string, unsigned> translatedTokens;
        std::shared_ptr<NewTokens> newTokens(new NewTokens(job_->file->tokenizer));
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
            if (isNew)
                (*newTokens)[id] = i.first;
            // create a record for the translated token and its uses
            translatedTokens[STR(std::hex << id)] = i.second;
            // swap translated tokens for the original tokens in the token map
            job_->tokens = std::move(translatedTokens);
            // if we have created any new tokens, pass them to the DBWriter.
            if (not newTokens->tokens.empty())
                DBWriter::Schedule(DBWriterJob(newTokens));
        }
    }

    void process(Context & context) {
        // let's first look if we have clone group
        getCloneGroup(context);
        // now let's convert tokens to their unique files
        translateTokensToIds(context);
    }

    virtual void process() {
        // check if the file has unique clone (this is independent of the tokenizer used)
        job_->file->fileHashUnique = hasUniqueFileHash(* job_->file);

        // get context for the used tokenizer and process tokens & clone pair information
        process(*contexts_[static_cast<unsigned>(job_->file->tokenizer)]);

        // pass the tokenized file to the db writer
        DBWriter::Schedule(DBWriterJob(job_->file));
        // and pass the tokens map to the writer if the file is unique
        if (job_->file->groupId == -1)
            Writer::Schedule(WriterJob(job_));
    }

    static std::mutex mFileHash_;
    static std::unordered_set<Hash> uniqueFileHashes_;
    static std::vector<Context * > contexts_;

};
