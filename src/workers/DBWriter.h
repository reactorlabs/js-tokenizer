#pragma once

#include <map>

#include <mysql/mysql.h>

#include "../data.h"
#include "../worker.h"

class DBWriterJob {
public:
    enum class Kind {
        Project,
        File,
        NewTokens,
        CloneGroup,
        TokenInfo
    };

    DBWriterJob() {

    }

    DBWriterJob(std::shared_ptr<ClonedProject> project):
        project(project) {
    }

    DBWriterJob(std::shared_ptr<TokenizedFile> file):
        file(file) {
    }

    DBWriterJob(std::shared_ptr<NewTokens> tokens):
        tokens(tokens) {
    }

    DBWriterJob(MergerStats * stats):
        mergerStats(stats) {
    }


    /* This can be optimized by union, but we then have to play the union destructor games, which is not fun. Also it should be enough since the queue sizes are bounded and therefore small.
     */
    std::shared_ptr<ClonedProject> project;
    std::shared_ptr<TokenizedFile> file;
    std::shared_ptr<NewTokens> tokens;
    std::shared_ptr<MergerStats> mergerStats;

    friend std::ostream & operator << (std::ostream & s, DBWriterJob const & j) {
        if (j.project != nullptr)
            s << "Project id " << j.project->id;
        else if (j.file != nullptr)
            s << "File id " << j.file->id << ", url " << j.file->project->url << "/" << j.file->relPath;
        else if (j.tokens != nullptr)
            s << "New tokens";
        else
            s << "Statistics";
        return s;
    }

};

class DBWriter : public Worker<DBWriterJob> {
public:
    DBWriter(unsigned index):
        Worker<DBWriterJob>("DB WRITER", index) {
        std::lock_guard<std::mutex> g(m_);
        c_ = mysql_init(nullptr);
        if (c_ == nullptr)
            throw STR("Unable to create MySQL connection");
        if (mysql_real_connect(c_, host_.c_str(), user_.c_str(), pass_.c_str(), db_.c_str(), 0, nullptr, 0) == nullptr)
            throw STR("Unable to connect to server " << host_ << ", db " << db_ << " using username " << user_ << " password_ " << pass_);
        // check we have properly created all tables
        checkTables();
    }

    static void SetConnection(std::string const & server, std::string const db, std::string const & username, std::string const password) {
        host_ = server;
        db_ = db;
        user_ = username;
        pass_ = password;
    }

    static void AddTokenizer(TokenizerKind k) {
        unsigned idx = static_cast<unsigned>(k);
        if (contexts_.size() < idx + 1)
            contexts_.resize(idx + 1);
        contexts_[idx].setPaths(prefix(k));
    }

protected:

    /** If the queue is idle, flush the buffers first, and then try again.

      This should in theory help with the congestion when most db threads start wtiting at the same time.
     */
    void getJob() override {
        if (jobs_.empty())
            flush();
        Worker<DBWriterJob>::getJob();
    }

private:


    struct Context {
        std::string tableProjects;
        std::string tableProjectsExtra;
        std::string tableFiles;
        std::string tableFilesExtra;
        std::string tableFileStats;
        std::string tableClonePairs;
        std::string tableCloneGroups;
        std::string tableTokenText;
        std::string tableTokens;

        void setPaths(std::string const & prefix) {
            tableProjects = "projects";
            tableProjectsExtra = "projects_extra";
            tableFiles = prefix + "files";
            tableFilesExtra = prefix + "files_extra";
            tableFileStats = prefix + "stats";
            tableClonePairs = prefix + "clone_pairs";
            tableCloneGroups = prefix + "clone_groups";
            tableTokenText = prefix + "token_text";
            tableTokens = prefix + "tokens";
        }
    };

    void insertInto(std::string const & table, std::string const & what, bool flush = false) {
        std::string & buffer = buffers_[table];
        if (not what.empty()) {
            if (buffer.empty())
                buffer = STR("INSERT INTO " << table << " VALUES (" << what << ")");
            else
                buffer = buffer + ",(" + what +")";
        }
        if ((flush and not buffer.empty()) or buffer.size() > 1024 * 1024) { // 10MB
            query(buffer);
            buffer = "";
        }
    }

    /** Writes all buffers to the database immediately.
     */
    void flush() {
        for (auto & i : buffers_) {
            insertInto(i.first, "", true);
        }
    }

    /** Writes the project info to the database.
     */
    void writeProject(ClonedProject const & p, Context const & c) {
        insertInto(c.tableProjects, STR(
            p.id <<
            ",NULL," <<
            escape(p.url)));
        insertInto(c.tableProjectsExtra, STR(
            p.id << "," <<
            p.createdAt));
    }


    void writeFile(TokenizedFile const & f, Context const & c) {
        insertInto(c.tableFiles, STR(
            f.id << "," <<
            f.project->id << "," <<
            escape(f.relPath) << "," <<
            escape(f.fileHash)));
        insertInto(c.tableFilesExtra, STR(
            f.id << "," <<
            f.createdAt));
        // if the file hash is unique, output it to the stats table
        if (f.fileHashUnique)
            insertInto(c.tableFileStats, STR(
                escape(f.fileHash) << "," <<
                f.bytes << "," <<
                f.lines << "," <<
                f.loc << "," <<
                f.sloc << "," <<
                f.totalTokens << "," <<
                f.uniqueTokens << "," <<
                escape(f.tokensHash)));
        // if the clone group is not -1 output the clone pair
        // Note this does not report clone group info for the first file in the group, this is written later when the clone groups are being finalized
        if (f.groupId != -1)
            insertInto(c.tableClonePairs, STR(
                f.id << "," <<
                f.groupId));
    }

    /** Writes the new tokens to the database.

      For each newly found token writes its id and the actual text.
     */
    void writeNewTokens(std::map<int, std::string> const & tokens, Context const & c) {
        for (auto const & i : tokens) {
            insertInto(c.tableTokenText, STR(
                i.first << "," <<
                i.second.size() << "," <<
                escape(i.second)));
        }
    }

    void writeCloneGroup(CloneGroup const * group, Context const & c) {
        insertInto(c.tableCloneGroups, STR(
            group->id << "," <<
            group->oldestId));
        // also make sure the first file in the group belongs to it
        insertInto(c.tableClonePairs, STR(
            group->id << "," <<
            group->id));
    }

    void writeTokenInfo(TokenInfo const * ti, Context const & c) {
        insertInto(c.tableTokens, STR(
            ti->id << "," <<
            ti->uses));
    }


    void process() override {
        // if it is project, use default context as projects table is identical for all tokenizers
        if (job_.project != nullptr)
            writeProject(* job_.project, contexts_[0]);
        // file's context is obtained from the file itself
        else if (job_.file != nullptr)
            writeFile(* job_.file, contexts_[static_cast<unsigned>(job_.file->tokenizer)]);
        // new tokens carry their context with them
        else if (job_.tokens != nullptr)
            writeNewTokens(job_.tokens->tokens, contexts_[static_cast<unsigned>(job_.tokens->tokenizer)]);
        else {
            assert (job_.mergerStats != nullptr);
            Context const & c = contexts_[static_cast<unsigned>(job_.mergerStats->tokenizer)];
            switch (job_.mergerStats->kind) {
                case MergerStats::Kind::CloneGroup:
                    writeCloneGroup(job_.mergerStats->cloneGroup, c);
                    break;
                case MergerStats::Kind::TokenInfo:
                    writeTokenInfo(job_.mergerStats->tokenInfo, c);
            }
        }
    }

    void query(std::string const & what) {
        int status = mysql_query(c_, what.c_str());
        if (status != 0)
            throw STR("SQL error on: " << what);
    }

    void resetDatabase(Context const & c) {
        if (c.tableProjects.empty())
            return; // unused context
        query(STR("DROP TABLE IF EXISTS " << c.tableProjects));
        query(STR("DROP TABLE IF EXISTS " << c.tableProjectsExtra));
        query(STR("DROP TABLE IF EXISTS " << c.tableFiles));
        query(STR("DROP TABLE IF EXISTS " << c.tableFilesExtra));
        query(STR("DROP TABLE IF EXISTS " << c.tableFileStats));
        query(STR("DROP TABLE IF EXISTS " << c.tableClonePairs));
        query(STR("DROP TABLE IF EXISTS " << c.tableCloneGroups));
        query(STR("DROP TABLE IF EXISTS " << c.tableTokens));
        query(STR("DROP TABLE IF EXISTS " << c.tableTokenText));
    }

    void resetDatabase() {
        for (Context & c : contexts_)
            resetDatabase(c);
    }

    void checkTables(Context const & c) {
        if (c.tableProjects.empty())
            return; // unused context
        // table for basic project information, compatible with sourcerer CC
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableProjects << " ("
            "projectId INT(6) NOT NULL,"
            "projectPath VARCHAR(4000) NULL,"
            "projectUrl VARCHAR(4000) NOT NULL,"
            "PRIMARY KEY (projectId),"
            "UNIQUE INDEX (projectId))"));
        // extra information about projects
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableProjectsExtra << " ("
            "projectId INT NOT NULL,"
            "createdAt INT UNSIGNED NOT NULL,"
            "PRIMARY KEY (projectId),"
            "UNIQUE INDEX (projectId))"));
        // table for all files found, compatible with sourcerer CC
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableFiles << " ("
            "fileId BIGINT(6) UNSIGNED NOT NULL,"
            "projectId INT(6) UNSIGNED NOT NULL,"
            "relativeUrl VARCHAR(4000) NOT NULL,"
            "fileHash CHAR(32) NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId),"
            "INDEX (projectId),"
            "INDEX (fileHash))"));
        // extra information about files
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableFilesExtra << " ("
            "fileId INT NOT NULL,"
            "createdAt INT UNSIGNED NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId))"));
        // statistics for unique files (based on *file* hash)
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableFileStats << " ("
            "fileHash CHAR(32) NOT NULL,"
            "fileBytes INT(6) UNSIGNED NOT NULL,"
            "fileLines INT(6) UNSIGNED NOT NULL,"
            "fileLOC INT(6) UNSIGNED NOT NULL,"
            "fileSLOC INT(6) UNSIGNED NOT NULL,"
            "totalTokens INT(6) UNSIGNED NOT NULL,"
            "uniqueTokens INT(6) UNSIGNED NOT NULL,"
            "tokenHash CHAR(32) NOT NULL,"
            "PRIMARY KEY (fileHash),"
            "UNIQUE INDEX (fileHash),"
            "INDEX (tokenHash))"));
        // tokenizer clone pairs (no counterpart in sourcerer CC)
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableClonePairs << " ("
            "fileId INT NOT NULL,"
            "groupId INT NOT NULL,"
            "PRIMARY KEY(fileId))"));
        // tokenizer clone groups (no counterpart in sourcerer CC)
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableCloneGroups << " ("
            "groupId INT NOT NULL,"
            "oldestId INT NOT NULL,"
            "PRIMARY KEY(groupId))"));
        // tokens and their freqencies (this is dumped once at the end of the run)
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableTokens << " ("
            "id INT NOT NULL,"
            "uses INT NOT NULL,"
            "PRIMARY KEY(id))"));
        // unique tokens, new tokens are added each time they are found
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableTokenText << " ("
            "id INT NOT NULL,"
            "size INT NOT NULL,"
            "text LONGTEXT NOT NULL,"
            "PRIMARY KEY(id))"));
    }


    void checkTables() {
        for (Context & c : contexts_)
            checkTables(c);
    }

    MYSQL * c_;

    std::map<std::string, std::string> buffers_;

    static std::string host_;
    static std::string user_;
    static std::string pass_;
    static std::string db_;

    static std::vector<Context> contexts_;

    static std::mutex m_;


};
