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

    friend std::ostream & operator << (std::ostream & s, MergerJob const & j) {
        s << "File id " << j->file->id << ", url: " << j->file->project->url << "/" << j->file->relPath;
        return s;
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

    /** Flushes the buffers making sure if they contain any data, this will be written to the database.
     */
    static void FlushBuffers() {
        for (auto & i : buffers_)
            i.second.flush();
    }

    static void FlushStatistics() {
        for (unsigned i = 0; i < contexts_.size(); ++i) {
            Context * c = contexts_[i];
            if (c == nullptr)
                continue;
            for (auto const & ii : c->cloneGroups) {
                buffers_[c->tableCloneGroups].append(STR(
                    ii.second.id << "," <<
                    ii.second.oldestId));
                buffers_[c->tableClonePairs].append(STR(
                    ii.second.id << "," <<
                    ii.second.id));
            }
            for (auto const & ii : c->tokenInfo) {
                buffers_[c->tableTokens].append(STR(
                    ii.second.id << "," <<
                    ii.second.uses));
            }
        }
        FlushBuffers();
    }

private:
    struct Context {
        std::unordered_map<Hash, CloneGroup> cloneGroups;
        std::unordered_map<Hash, TokenInfo> tokenInfo;
        std::unordered_set<Hash> uniqueFileHashes;
        std::mutex mCloneGroups;
        std::mutex mTokenInfo;
        std::mutex mFileHash;

        std::string tableFiles;
        std::string tableFilesExtra;
        std::string tableFileStats;
        std::string tableClonePairs;
        std::string tableCloneGroups;
        std::string tableTokenText;
        std::string tableTokens;

        Context(TokenizerKind k) {
            std::string p = prefix(k);
            tableFiles = p + "files";
            tableFilesExtra = p + "files_extra";
            tableFileStats = p + "stats";
            tableClonePairs = p + "clone_pairs";
            tableCloneGroups = p + "clone_groups";
            tableTokenText = p + "token_text";
            tableTokens = p + "tokens";
            Merger::buffers_[tableFiles].setTableName(tableFiles);
            Merger::buffers_[tableFilesExtra].setTableName(tableFilesExtra);
            Merger::buffers_[tableFileStats].setTableName(tableFileStats);
            Merger::buffers_[tableClonePairs].setTableName(tableClonePairs);
            Merger::buffers_[tableCloneGroups].setTableName(tableCloneGroups);
            Merger::buffers_[tableTokenText].setTableName(tableTokenText);
            Merger::buffers_[tableTokens].setTableName(tableTokens);
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
        DBBuffer & b = buffers_[context.tableTokenText];
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
            if (isNew)
                b.append(STR(
                     id << "," <<
                     i.first.size() << "," <<
                     escape(i.first)));
            // create a record for the translated token and its uses
            translatedTokens[STR(std::hex << id)] = i.second;
        }
        // swap translated tokens for the original tokens in the token map
        job_->tokens = std::move(translatedTokens);
    }

    void process(Context & c) {
        // first store the file
        buffers_[c.tableFiles].append(STR(
            job_->file->id << "," <<
            job_->file->project->id << "," <<
            escape(job_->file->relPath) << "," <<
            escape(job_->file->fileHash)));
        // and its extra information
        buffers_[c.tableFilesExtra].append(STR(
            job_->file->id << "," <<
            job_->file->createdAt));
        // if the file has unique hash output also its statistics
        if (hasUniqueFileHash(* job_->file, c)) {
            buffers_[c.tableFileStats].append(STR(
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
        if (groupId != -1) {
            buffers_[c.tableClonePairs].append(STR(
                job_->file->id << "," <<
                groupId));
            // also pass the file to the writer so that sourcerer output is written
            Writer::Schedule(WriterJob(job_));
        }
    }

    virtual void process() {
        // get context for the used tokenizer and process tokens & clone pair information
        process(*contexts_[static_cast<unsigned>(job_->file->tokenizer)]);
    }

    static std::vector<Context * > contexts_;

    static std::map<std::string, DBBuffer> buffers_;

};
