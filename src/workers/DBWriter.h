#pragma once

#include <map>

#include <mysql/mysql.h>

#include "../data.h"
#include "../worker.h"

class RAII {
public:
    RAII(std::function<void(void)> f):
        f_(f) {
    }

    ~RAII() {
        f_();
    }
private:
    std::function<void(void)> f_;


};

class SQLConnection {
public:

    static void SetConnection(std::string const & server, std::string const & user, std::string const & pass) {
        server_ = server;
        user_ = user;
        pass_ = pass;
    }

    SQLConnection() {
        c_ = mysql_init(nullptr);
        if (c_ == nullptr)
            throw STR("Unable to create sql connection");
        if (mysql_real_connect(c_, server_.c_str(), user_.c_str(), pass_.c_str(), nullptr, 0, nullptr, 0) == nullptr)
            throw STR("Unable to connect to sql server " << server_ << " using given credentials: " << mysql_error(c_));
    }

    ~SQLConnection() {
        mysql_close(c_);
    }

    void query(std::string const & q) {
        if (mysql_query(c_, q.c_str()) != 0)
            throw STR("SQL error: " << mysql_error(c_) << " in query: " << q);
    }

    void query(std::string const & q, std::function<void(unsigned, char**)> f) {
        query(q);
        MYSQL_RES * result = mysql_store_result(c_);
        if (result == nullptr)
            throw STR("SQL error: " << mysql_error(c_) << " when getting result of: " << q);
        // make sure we free result whatever happens
        RAII raii([result]() { mysql_free_result(result); });
        unsigned numFields = mysql_num_fields(result);
        // now process the query, calling f on each row
        MYSQL_ROW row;
        while (row = mysql_fetch_row(result))
            f(numFields, row);
    }
private:
    MYSQL * c_;

    static std::string server_;
    static std::string user_;
    static std::string pass_;
};


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

class DBWriter : public Worker<DBWriterJob>, SQLConnection {
public:
    static char const * Name() {
        return "DB WRITER";
    }
    DBWriter(unsigned index):
        Worker<DBWriterJob>(Name(), index),
        SQLConnection() {
        query(STR("USE " << db_));
    }

    static std::string & DatabaseName() {
        return db_;
    }

    static std::string const TableStamp;
    static std::string const TableProjects;
    static std::string const TableProjectsExtra;
    static std::string const TableFiles;
    static std::string const TableFilesExtra;
    static std::string const TableStats;
    static std::string const TableClonePairs;
    static std::string const TableCloneGroups;
    static std::string const TableTokensText;
    static std::string const TableTokens;
    static std::string const TableTokenHashes;
    static std::string const TableCloneGroupHashes;



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


    static std::string db_;

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
