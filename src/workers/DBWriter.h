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

    void reconnect() {
        mysql_close(c_);
        c_ = mysql_init(nullptr);
        if (c_ == nullptr)
            throw STR("Unable to create sql connection");
        if (mysql_real_connect(c_, server_.c_str(), user_.c_str(), pass_.c_str(), nullptr, 0, nullptr, 0) == nullptr)
            throw STR("Unable to connect to sql server " << server_ << " using given credentials: " << mysql_error(c_));
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

/*    static std::string TableName(TokenizerKind tk, Buffer::Kind k) {
        std::string result = prefix(tk);
        switch (k) {
            case Buffer::Kind::Projects:
                return "projects";
            case Buffer::Kind::ProjectsExtra:
                return "projects_extra";
            case Buffer::Kind::Files:
                return "files";
            case Buffer::Kind::FilesExtra:
                return "files_extra";
            case Buffer::Kind::Stats:
                return result + "stats";
            case Buffer::Kind::ClonePairs:
                return result + "clone_pairs";
            case Buffer::Kind::CloneGroups:
                return result + "clone_groups";
            case Buffer::Kind::Tokens:
                return result + "tokens";
            case Buffer::Kind::TokensText:
                return result + "tokens_text";
            default:
                throw STR("Buffer kind not supported for DB target");
        }
    } */

    static std::string & DatabaseName() {
        return db_;
    }

/*    static std::string const TableStamp;
    static std::string const TableSummary;
    */


private:



    void process() override {
        if (queries_++ == 10) {
            reconnect();
            query(STR("USE " << db_));
        }
        try {
            // if it is project, use default context as projects table is identical for all tokenizers
            query(job_.buffer);
        } catch (std::string const & e) {
            reconnect();
            query(STR("USE " << db_));
            throw e;
        }
    }

    unsigned queries_ = 0;


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
