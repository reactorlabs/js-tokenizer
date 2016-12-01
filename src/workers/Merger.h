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
                // clone group hash for snapshoting
                buffers_[c->tableCloneGroupHashes].append(STR(
                    ii.second.id << "," <<
                    escape(ii.first)));
            }
            for (auto const & ii : c->tokenInfo) {
                buffers_[c->tableTokens].append(STR(
                    ii.second.id << "," <<
                    ii.second.uses));
                // token info hash for snapshoting
                buffers_[c->tableTokenHashes].append(STR(
                    ii.second.id << "," <<
                    escape(ii.first)));
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
        // num of files with unique token hashes (i.e. those that would go to sourcererCC)
        std::atomic_uint uniqueTokenHashes;

        std::string tableFiles;
        std::string tableFilesExtra;
        std::string tableStats;
        std::string tableClonePairs;
        std::string tableCloneGroups;
        std::string tableTokensText;
        std::string tableTokens;
        std::string tableTokenHashes;
        std::string tableCloneGroupHashes;

        Context(TokenizerKind k) {
            std::string p = prefix(k);
            tableFiles = p + DBWriter::TableFiles;
            tableFilesExtra = p + DBWriter::TableFilesExtra;
            tableStats = p + DBWriter::TableStats;
            tableClonePairs = STR(p << DBWriter::TableClonePairs << "_" << ClonedProject::StrideIndex());
            tableCloneGroups = STR(p << DBWriter::TableCloneGroups << "_" << ClonedProject::StrideIndex());
            tableTokensText = STR(p << DBWriter::TableTokensText << "_" << ClonedProject::StrideIndex());
            tableTokens = STR(p << DBWriter::TableTokens << "_" << ClonedProject::StrideIndex());
            tableTokenHashes = STR(p << DBWriter::TableTokenHashes << "_" << ClonedProject::StrideIndex());
            tableCloneGroupHashes = STR(p << DBWriter::TableCloneGroupHashes << "_" << ClonedProject::StrideIndex());
            Merger::buffers_[tableFiles].setTableName(tableFiles);
            Merger::buffers_[tableFilesExtra].setTableName(tableFilesExtra);
            Merger::buffers_[tableStats].setTableName(tableStats);
            Merger::buffers_[tableClonePairs].setTableName(tableClonePairs);
            Merger::buffers_[tableCloneGroups].setTableName(tableCloneGroups);
            Merger::buffers_[tableTokensText].setTableName(tableTokensText);
            Merger::buffers_[tableTokens].setTableName(tableTokens);
            Merger::buffers_[tableTokenHashes].setTableName(tableTokenHashes);
            Merger::buffers_[tableCloneGroupHashes].setTableName(tableCloneGroupHashes);
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
        DBBuffer & b = buffers_[context.tableTokensText];
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
                         escape(STR(i.first.substr(0, 1000) << "......" << i.first.substr(i.first.size() - 1000)))));

                } else {
                    b.append(STR(
                         id << "," <<
                         i.first.size() << "," <<
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
            buffers_[c.tableStats].append(STR(
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
            // pass the file to the writer so that sourcerer output is written
            Writer::Schedule(WriterJob(job_));
            // increase number of unique token hashes
            ++c.uniqueTokenHashes;
        } else {
            // emit the clone pair
            buffers_[c.tableClonePairs].append(STR(
                job_->file->id << "," <<
                groupId));
        }
    }

    virtual void process() {
        // get context for the used tokenizer and process tokens & clone pair information
        process(*contexts_[static_cast<unsigned>(job_->file->tokenizer)]);
    }

    static std::vector<Context * > contexts_;

    static std::map<std::string, DBBuffer> buffers_;

};
