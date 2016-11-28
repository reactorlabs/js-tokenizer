#pragma once

#include <map>

#include <mysql/mysql.h>

#include "../data.h"
#include "../worker.h"

class DBWriterJob {
public:
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

    DBWriterJob(std::shared_ptr<MergerStats> tokens):
        mergerStats(tokens) {
    }

    std::shared_ptr<ClonedProject> project;
    std::shared_ptr<TokenizedFile> file;
    std::shared_ptr<NewTokens> tokens;
    std::shared_ptr<MergerStats> mergerStats;
};

class DBWriter : public Worker<DBWriterJob> {
public:
    DBWriter(unsigned index):
        Worker<DBWriterJob>("DB WRITER", index) {
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

    /** Writes the project info to the database.
     */
    void writeProject(ClonedProject const & p, Context const & c) {
        query(STR("INSERT INTO " << c.tableProjects << " VALUES (" <<
            p.id << "," <<
            "NULL," << // we do not keep the files on disk
            escape(p.url) << ")"));
        // now write the extra information we keep about the project
        query(STR("INSERT INTO " << c.tableProjectsExtra << " VALUES (" <<
            p.id << "," <<
            p.createdAt << ")"));
    }


    void writeFile(TokenizedFile const & f, Context const & c) {
        // each file goes to the files table
        query(STR("INSERT INTO " << c.tableFiles << " VALUES (" <<
            f.id << "," <<
            f.project->id << "," <<
            escape(f.relPath) << "," <<
            escape(f.fileHash) << ")"));
        // extra file information
        query(STR("INSERT INTO " << c.tableFilesExtra << " VALUES (" <<
            f.id << "," <<
            f.createdAt << ")"));
        // now if the file has unique file hash, export also its statistics
        if (f.fileHashUnique) {
            query(STR("INSERT INTO " << c.tableFileStats << " VALUES (" <<
                escape(f.fileHash) << "," <<
                f.bytes << "," <<
                f.lines << "," <<
                f.loc << "," <<
                f.sloc << "," <<
                f.totalTokens << "," <<
                f.uniqueTokens << "," <<
                escape(f.tokensHash) << ")"));
        }
        // if the clone group is not -1 output the clone pair
        if (f.groupId != -1) {
            // Note this does not report clone group info for the first file in the group, this is written later when the clone groups are being finalized
            query(STR("INSERT INTO " << c.tableClonePairs << " VALUES (" <<
                f.id << "," <<
                f.groupId << ")"));
        }
    }

    /** Writes the new tokens to the database.

      For each newly found token writes its id and the actual text.
     */
    void writeNewTokens(std::map<int, std::string> const & tokens, Context const & c) {
        auto i = tokens.begin(), e = tokens.end();
        while (i != e) {
            std::stringstream ss;
            ss << "INSERT INTO " << c.tableTokenText << " VALUES ";
            ss << " (" << i->first << "," << escape(i->second) << ")";
            unsigned approxSize = i->second.size();
            while (++i != e and approxSize < 10000) {
                ss << ",(" << i->first << "," << escape(i->second) << ")";
                approxSize += i->second.size();
            }
            query(ss.str());
            ss.clear();
        }
    }

    void writeCloneGroups(std::map<Hash, CloneGroup> const & groups, Context const & c) {
        auto i = groups.begin(), e = groups.end();
        while (i != e) {
            std::stringstream sg;
            std::stringstream cp;
            sg << "INSERT INTO " << c.tableCloneGroups << " VALUES ";
            cp << "INSERT INTO " << c.tableClonePairs << " VALUES ";
            sg << "(" << i->second.id << "," << i->second.oldestId << ")";
            cp << "(" << i->second.id << "," << i->second.id << ")";
            unsigned ii = 1000;
            while (++i != e and --ii != 0) {
                sg << ",(" << i->second.id << "," << i->second.oldestId << ")";
                cp << ",(" << i->second.id << "," << i->second.id << ")";
            }
            query(sg.str());
            query(cp.str());
            sg.clear();
            cp.clear();
        }
    }

    void writeTokenInfo(std::map<Hash, TokenInfo> const & tokens, Context const & c) {
        auto i = tokens.begin(), e = tokens.end();
        while (i != e) {
            std::stringstream ss;
            ss << "INSERT INTO " << c.tableTokens << " VALUES ";
            ss << "(" << i->second.id << "," << i->second.size << "," << i->second.uses << ")";
            unsigned ii = 1000;
            while (++i != e and ii-- != 0)
                ss << ",(" << i->second.id << "," << i->second.size << "," << i->second.uses << ")";
            query(ss.str());
            ss.clear();
        }
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
            writeCloneGroups(job_.mergerStats->cloneGroups, c);
            writeTokenInfo(job_.mergerStats->tokens, c);
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
            "UNIQUE INDEX (projectId)) ENGINE = MYISAM"));
        // extra information about projects
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableProjectsExtra << " ("
            "projectId INT NOT NULL,"
            "createdAt INT UNSIGNED NOT NULL,"
            "PRIMARY KEY (projectId),"
            "UNIQUE INDEX (projectId)) ENGINE = MYISAM"));
        // table for all files found, compatible with sourcerer CC
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableFiles << " ("
            "fileId BIGINT(6) UNSIGNED NOT NULL,"
            "projectId INT(6) UNSIGNED NOT NULL,"
            "relativePath VARCHAR(4000) NULL,"
            "relativeUrl VARCHAR(4000) NOT NULL,"
            "fileHash CHAR(32) NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId),"
            "INDEX (projectId),"
            "INDEX (fileHash)) ENGINE = MYISAM"));
        // extra information about files
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableFilesExtra << " ("
            "fileId INT NOT NULL,"
            "createdAt INT UNSIGNED NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId)) ENGINE = MYISAM"));
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
            "INDEX (tokenHash)) ENGINE = MYISAM"));
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
            "size INT NOT NULL,"
            "uses INT NOT NULL,"
            "PRIMARY KEY(id))"));

        // unique tokens, new tokens are added each time they are found
        query(STR("CREATE TABLE IF NOT EXISTS " << c.tableTokenText << " ("
            "id INT NOT NULL,"
            "text LONGTEXT NOT NULL,"
            "PRIMARY KEY(id))"));
    }

    void checkTables() {
        for (Context & c : contexts_)
            checkTables(c);
    }



    MYSQL * c_;


    static std::string host_;
    static std::string user_;
    static std::string pass_;
    static std::string db_;

    static std::vector<Context> contexts_;


};
