#pragma once

#include <map>

#include <mysql/mysql.h>

#include "../data.h"
#include "../worker.h"

class DBWriterJob {
public:

    DBWriterJob(std::string && buffer):
        buffer(std::move(buffer)) {
    }

    DBWriterJob() {

    }

    std::string buffer;

    friend std::ostream & operator << (std::ostream & s, DBWriterJob const & j) {
        s << j.buffer.substr(0, 100) << "...";
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
    }

    static void SetConnection(std::string const & server, std::string const db, std::string const & username, std::string const password) {
        host_ = server;
        db_ = db;
        user_ = username;
        pass_ = password;
    }

    static void ResetDatabase() {
        DBWriter db(123456);
        db.deleteTables();
        db.createTables();

    }


private:

    void deleteTables() {
        query(STR("DROP TABLE IF EXISTS projects"));
        query(STR("DROP TABLE IF EXISTS projects_extra"));
        std::string p = prefix(TokenizerKind::Generic);
        query(STR("DROP TABLE IF EXISTS " << p << "files"));
        query(STR("DROP TABLE IF EXISTS " << p << "files_extra"));
        query(STR("DROP TABLE IF EXISTS " << p << "stats"));
        query(STR("DROP TABLE IF EXISTS " << p << "clone_pairs"));
        query(STR("DROP TABLE IF EXISTS " << p << "clone_groups"));
        query(STR("DROP TABLE IF EXISTS " << p << "token_text"));
        query(STR("DROP TABLE IF EXISTS " << p << "tokens"));
        p = prefix(TokenizerKind::JavaScript);
        query(STR("DROP TABLE IF EXISTS " << p << "files"));
        query(STR("DROP TABLE IF EXISTS " << p << "files_extra"));
        query(STR("DROP TABLE IF EXISTS " << p << "stats"));
        query(STR("DROP TABLE IF EXISTS " << p << "clone_pairs"));
        query(STR("DROP TABLE IF EXISTS " << p << "clone_groups"));
        query(STR("DROP TABLE IF EXISTS " << p << "token_text"));
        query(STR("DROP TABLE IF EXISTS " << p << "tokens"));
    }

    void createTables() {
        // table for basic project information, compatible with sourcerer CC
        query(STR("CREATE TABLE IF NOT EXISTS projects ("
            "projectId INT(6) NOT NULL,"
            "projectPath VARCHAR(4000) NULL,"
            "projectUrl VARCHAR(4000) NOT NULL,"
            "PRIMARY KEY (projectId),"
            "UNIQUE INDEX (projectId))"));
        // extra information about projects
        query(STR("CREATE TABLE IF NOT EXISTS projects_extra ("
            "projectId INT NOT NULL,"
            "createdAt INT UNSIGNED NOT NULL,"
            "PRIMARY KEY (projectId),"
            "UNIQUE INDEX (projectId))"));
        createTables(prefix(TokenizerKind::Generic));
        createTables(prefix(TokenizerKind::JavaScript));
    }

    void createTables(std::string const & p) {
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "files ("
            "fileId BIGINT(6) UNSIGNED NOT NULL,"
            "projectId INT(6) UNSIGNED NOT NULL,"
            "relativeUrl VARCHAR(4000) NOT NULL,"
            "fileHash CHAR(32) NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId),"
            "INDEX (projectId),"
            "INDEX (fileHash))"));
        // extra information about files
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "files_extra ("
            "fileId INT NOT NULL,"
            "createdAt INT UNSIGNED NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId))"));
        // statistics for unique files (based on *file* hash)
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "stats ("
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
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "clone_pairs ("
            "fileId INT NOT NULL,"
            "groupId INT NOT NULL,"
            "PRIMARY KEY(fileId))"));
        // tokenizer clone groups (no counterpart in sourcerer CC)
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "clone_groups ("
            "groupId INT NOT NULL,"
            "oldestId INT NOT NULL,"
            "PRIMARY KEY(groupId))"));
        // tokens and their freqencies (this is dumped once at the end of the run)
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "tokens ("
            "id INT NOT NULL,"
            "uses INT NOT NULL,"
            "PRIMARY KEY(id))"));
        // unique tokens, new tokens are added each time they are found
        query(STR("CREATE TABLE IF NOT EXISTS " << p << "token_text ("
            "id INT NOT NULL,"
            "size INT NOT NULL,"
            "text LONGTEXT NOT NULL,"
            "PRIMARY KEY(id))"));
    }


    void process() override {
        // if it is project, use default context as projects table is identical for all tokenizers
        query(job_.buffer);
    }

    void query(std::string const & what) {
        int status = mysql_query(c_, what.c_str());
        if (status != 0)
            throw STR("SQL error on: " << what);
    }

    MYSQL * c_;

    static std::string host_;
    static std::string user_;
    static std::string pass_;
    static std::string db_;

    static std::mutex m_;

};

class DBBuffer {
public:
    static const unsigned BUFFER_LIMIT = 1024 * 1024; // 1mb

    DBBuffer(std::string const & name = ""):
        tableName_(name) {
    }

    void append(std::string const & what) {
        std::lock_guard<std::mutex> g(m_);
        if (buffer_.empty())
            buffer_ = STR("INSERT INTO " << tableName_ << " VALUES (" << what << ")");
        else
            buffer_ += STR(",(" << what << ")");
        if (buffer_.size() > BUFFER_LIMIT)
            guardedFlush();
    }

    void flush() {
        std::lock_guard<std::mutex> g(m_);
        if (not buffer_.empty())
            guardedFlush();
    }

    void setTableName(std::string const & value) {
        assert (tableName_.empty() or tableName_ == value);
        tableName_ = value;
    }

private:
    void guardedFlush() {
        DBWriter::Schedule(DBWriterJob(std::move(buffer_)));
        assert(buffer_.empty());
    }

    std::mutex m_;
    std::string buffer_;
    std::string tableName_;
};
