#pragma once

#include <fstream>
#include <unordered_map>

#include "worker.h"
#include "tokenizer.h"
#include "data.h"

#include <mysql/mysql.h>



template<TokenizerType TYPE>
struct WriterJob {
    enum class Kind {
        File,
        Project,
        NewToken,
        TokenInfo,
        CloneGroups
    };
    Kind kind;
    // this is wasteful, we can make union of it, but should not cost us much
    int id;
    std::map<int, std::string> tokens;
    union {
        TokenizedFile *f;
        ClonedProject *p;
        std::unordered_map<hash, ::CloneInfo> *cloneGroups;
        std::unordered_map<hash, ::TokenInfo> *tokenInfo;
    };

    static WriterJob File(TokenizedFile * f) {
        return WriterJob(f);
    }

    static WriterJob Project(ClonedProject * p) {
        return WriterJob(p);
    }

    static WriterJob NewToken(std::map<int, std::string> const & tokens) {
        return WriterJob(tokens);
    }

    static WriterJob TokenInfo(std::unordered_map<hash, ::TokenInfo> * tokens) {
        return WriterJob(tokens);
    }

    static WriterJob CloneGroups(std::unordered_map<hash, ::CloneInfo> * cloneGroups) {
        return WriterJob(cloneGroups);
    }

    friend std::ostream & operator << (std::ostream & s, WriterJob const & job) {
        switch (job.kind) {
            case WriterJob::Kind::File:
                s << "File id " << job.f->id << ", path: ", job.f->path();
                break;
            case WriterJob::Kind::Project:
                s << "Project id " << job.p->id << ", path: ", job.p->path();
                break;
            case WriterJob::Kind::NewToken:
                s << "NewTokens " << job.tokens.size();
                break;
            case WriterJob::Kind::TokenInfo:
                s << "Token info ";
                break;
            case WriterJob::Kind::CloneGroups:
                s << "Clone group info id";
                break;
            default:
                s << "Unknown";
        }
    }
private:

    WriterJob(TokenizedFile * f):
        kind(Kind::File),
        f(f) {
    }

    WriterJob(ClonedProject * p):
        kind(Kind::Project),
        p(p) {
    }

    WriterJob(std::map<int, std::string> const & tokens):
        kind(Kind::NewToken),
        tokens(tokens) {
    }

    WriterJob(std::unordered_map<hash, ::TokenInfo> * tokens):
        kind(Kind::TokenInfo),
        tokenInfo(tokens) {
    }

    WriterJob(std::unordered_map<hash, ::CloneInfo> * cloneGroups):
        kind(Kind::CloneGroups),
        cloneGroups(cloneGroups) {

    }

};

class DBConnection {
public:
    static void setConnection(std::string const & host, std::string user, std::string pass, std::string db) {
        host_ = host;
        user_ = user;
        pass_ = pass;
        db_ = db;
    }

    static void createDatabase(bool reset = false) {
        DBConnection c;
        try {
            if (reset)
                c.query(STR("DROP DATABASE " << db_));
            c.query(STR("USE " << db_));
        } catch (...) {
            Worker::Print(STR("Database " << db_ << "not found, creating..."));
            c.query(STR("CREATE DATABASE " << db_));
        }
    }



    ~DBConnection() {
        mysql_close(c_);
    }

protected:

    DBConnection() {
        c_ = mysql_init(nullptr);
        if (c_ == nullptr)
            throw STR("Unable to create MySQL connection");
        if (mysql_real_connect(c_, host_.c_str(), user_.c_str(), pass_.c_str(), nullptr, 0, nullptr, 0) == nullptr)
            throw STR("Unable to connect to database " << host_ << " using username " << user_ << " password_ " << pass_);
    }


     void query(std::string const & what) {
        //Worker::Print(what);
        int status = mysql_query(c_, what.c_str());
        if (status != 0)
            throw STR("Error executing statement " << what);
    }







    MYSQL * c_;



    static std::string host_;
    static std::string user_;
    static std::string pass_;
    static std::string db_;



};

template<TokenizerType TYPE>
class Writer : public DBConnection, public QueueWorker<WriterJob<TYPE>> {
public:

    Writer(unsigned index):
        QueueWorker<WriterJob<TYPE >>(STR("WRITER_" << TYPE), index), DBConnection() {
        query(STR("USE " << db_));
        checkTables();

    }

    static void InitializeSourcererOutput(std::string const & outputDir) {
        createDirectory(outputDir);
        std::string filename = STR(outputDir << "/" << tablePrefix() << "blocks.file");
        sourcererOutput.open(filename);
        if (not sourcererOutput.good())
            throw STR("Unable to open file " << filename << " for writing");
    }


private:

    using QueueWorker<WriterJob<TYPE>>::Print;

    static std::string tablePrefix() {
        switch (TYPE) {
            case TokenizerType::Generic:
                return "";
            case TokenizerType::JavaScript:
                return "js_";
            default:
                throw STR("Not Implemented");
        }
    }


    void checkTables() {
        // all projects go here
        query(STR("CREATE TABLE IF NOT EXISTS " << tableProjects << " ("
            "projectId INT(6),"
            "projectPath VARCHAR(4000) NULL,"
            "projectUrl VARCHAR(4000) NOT NULL,"
            "PRIMARY KEY (projectId),"
            "UNIQUE INDEX (projectId)) ENGINE = MYISAM"));
        // all files go here
        query(STR("CREATE TABLE IF NOT EXISTS " << tableFiles << " ("
            "fileId BIGINT(6) UNSIGNED NOT NULL,"
            "projectId INT(6) UNSIGNED NOT NULL,"
            "relativePath VARCHAR(4000) NULL,"
            "relativeUrl VARCHAR(4000) NOT NULL,"
            "fileHash CHAR(32) NOT NULL,"
            "PRIMARY KEY (fileId),"
            "UNIQUE INDEX (fileId),"
            "INDEX (projectId),"
            "INDEX (fileHash)) ENGINE = MYISAM"));
        // files with unique *token* hash go here
        query(STR("CREATE TABLE IF NOT EXISTS " << tableStats << " ("
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

        // tokens and their freqencies (this is dumped once at the end of the run)
        query(STR("CREATE TABLE IF NOT EXISTS " << tableTokens << " ("
            "id INT NOT NULL,"
            "size INT NOT NULL,"
            "uses INT NOT NULL,"
            "PRIMARY KEY(id))"));

        // unique tokens, new tokens are added each time they are found
        query(STR("CREATE TABLE IF NOT EXISTS " << tableTokenTexts << " ("
            "id INT NOT NULL,"
            "text LONGTEXT NOT NULL,"
            "PRIMARY KEY(id))"));

        // tokenizer clone pairs
        query(STR("CREATE TABLE IF NOT EXISTS " << tableClonePairs << " ("
            "fileId INT NOT NULL,"
            "groupId INT NOT NULL,"
            "PRIMARY KEY(fileId))"));

        query(STR("CREATE TABLE IF NOT EXISTS " << tableCloneGroups << " ("
            "groupId INT NOT NULL,"
            "oldestId INT NOT NULL,"
            "PRIMARY KEY(groupId))"));


/*
        query(STR("CREATE TABLE IF NOT EXISTS " << tableProjects << " ("
            "id INT NOT NULL,"
            "url VARCHAR(255) NOT NULL,"
            "createdAt INT NOT NULL,"
            "PRIMARY KEY(id))"));
        query(STR("CREATE TABLE IF NOT EXISTS " << tableFiles << " ("
            "id INT NOT NULL,"
            "projectId INT NOT NULL,"
            "relPath VARCHAR(255) NOT NULL,"
            "fileHash CHARACTER(32) NOT NULL,"
            "tokenHash CHARACTER(32) NOT NULL,"
            "bytes INT NOT NULL,"
            "numLines INT NOT NULL,"
            "loc INT NOT NULL,"
            "sloc INT NOT NULL,"
            "totalTokens INT NOT NULL,"
            "uniqueTokens INT NOT NULL,"
            "createdAt INT NOT NULL,"
            "PRIMARY KEY(id))"));
        query(STR("CREATE TABLE IF NOT EXISTS " << tableClonedFiles << " ("
            "id INT NOT NULL,"
            "projectId INT NOT NULL,"
            "relPath VARCHAR(255) NOT NULL,"
            "fileHash CHARACTER(32) NOT NULL,"
            "filesId INT NOT NULL,"
            "PRIMARY KEY(id))"));
        query(STR("CREATE TABLE IF NOT EXISTS " << tableTokens << " ("
            "id INT NOT NULL,"
            "size INT NOT NULL,"
            "uses INT NOT NULL,"
            "PRIMARY KEY(id))"));
        query(STR("CREATE TABLE IF NOT EXISTS " << tableTokenTexts << " ("
            "id INT NOT NULL,"
            "text LONGTEXT NOT NULL,"
            "PRIMARY KEY(id))")); */
    }


    using QueueWorker<WriterJob<TYPE>>::processedFiles_;

    void newToken(std::map<int, std::string> const & tokens) {
        std::stringstream ss;
        ss << "INSERT INTO " << tableTokenTexts << " VALUES ";
        auto i = tokens.begin(), e = tokens.end();
        ss << " (" << i->first << "," << escape(i->second) << ")";
        while (++i != e)
            ss << ", (" << i->first << "," << escape(i->second) << ")";
        query(ss.str());
//            query(STR("INSERT INTO " << tableTokenTexts << " VALUES (" << i.first << ", " << escape(i.second) << ")"));
    }

    void file(TokenizedFile & f) {
        // each file goes to files table
        query(STR("INSERT INTO " << tableFiles << " VALUES (" <<
            f.id << "," <<
            f.project->id << "," <<
            "NULL, " <<
            escape(f.relPath) << "," <<
            escape(f.fileHash) << ")"));

        // any file with unique file hash should go into the stats table
        if (f.fileHashUnique) {
            query(STR("INSERT INTO " << tableStats << " VALUES (" <<
                escape(f.fileHash) << "," <<
                f.bytes << "," <<
                f.loc << "," <<
                (f.loc - f.emptyLoc) << "," <<
                (f.loc - f.emptyLoc - f.commentLoc) << "," <<
                f.totalTokens << "," <<
                f.tokens.size() << "," <<
                escape(f.tokensHash) << ")"));
        }

        if (f.cloneGroupId == -1) {
            if (not f.tokens.empty()) {
                std::lock_guard<std::mutex> g(m_);
                sourcererOutput <<
                                f.project->id << "," <<
                                f.id << "," <<
                                f.totalTokens << "," <<
                                f.tokens.size() << "," <<
                                f.tokensHash << "@#@";
                auto i = f.tokens.begin(), e = f.tokens.end();
                sourcererOutput << i->first << "@@::@@" << i->second;
                ++i;
                while (i != e) {
                    sourcererOutput << "," << i->first << "@@::@@" << i->second;
                    ++i;
                }
                sourcererOutput << std::endl;
            }
        } else {
            query(STR("INSERT INTO " << tableClonePairs << " VALUES (" <<
                  f.id << "," <<
                  f.cloneGroupId << ")"));
        }

        // detach from the project and delete the file information as we won't be needing it anymore
        f.project->release();
        delete &f;
        ++processedFiles_;
    }

    void project(ClonedProject &p) {
        query(STR("INSERT INTO " << tableProjects << " VALUES (" <<
              p.id << "," <<
              "NULL," <<
              escape(p.url) << ")"));
        p.release();
    }

    void tokenInfo(std::unordered_map<hash, ::TokenInfo> & tokens) {
        auto i = tokens.begin(), e = tokens.end();
        while (i != e) {
            std::stringstream ss;
            ss << "INSERT INTO " << tableTokens << " VALUES ";
            ss << "(" << i->second.id << "," << i->second.textSize << "," << i->second.uses << ")";
            unsigned x = 1000;
            while (--x > 0 and ++i != e)
                ss << ", (" << i->second.id << "," << i->second.textSize << "," << i->second.uses << ")";
            if (i != e)
                ++i;
            query(ss.str());
            ss.clear();
        }
/*        query(STR("INSERT INTO " << tableTokens << " VALUES (" <<
            id << ", " <<
            size << ", " <<
            uses << ")")); */
    }

    void cloneGroups(std::unordered_map<hash, ::CloneInfo> & cloneGroups) {
        auto i = cloneGroups.begin(), e = cloneGroups.end();
        while (i != e) {
            std::stringstream ss;
            ss << "INSERT INTO " << tableCloneGroups << " VALUES ";
            ss << "(" << i->second.id << ", " << i->second.oldestId << ")";
            unsigned x = 1000;
            while (--x > 0 and ++i != e)
                ss << ", (" << i->second.id << "," << i->second.oldestId << ")";
            if (i != e)
                ++i;
            query(ss.str());
            ss.clear();
        }
        /*query(STR("INSERT INTO " << tableCloneGroups << " VALUES (" <<
              id << "," << oldestId << ")"));
              */
    }

    void process(WriterJob<TYPE> const & job) override {
        switch (job.kind) {
            case WriterJob<TYPE>::Kind::File:
                file(*job.f);
                break;
            case WriterJob<TYPE>::Kind::Project:
                project(*job.p);
                break;
            case WriterJob<TYPE>::Kind::NewToken:
                newToken(job.tokens);
                break;
            case WriterJob<TYPE>::Kind::TokenInfo:
                tokenInfo(* job.tokenInfo);
                break;
            case WriterJob<TYPE>::Kind::CloneGroups:
                cloneGroups(* job.cloneGroups);
                break;
            default:
                throw STR("Unknown job");
        }
    }

    static std::string const tableProjects;

    static std::string const tableFiles;

    static std::string const tableStats;

    static std::string const tableClonePairs;

    static std::string const tableCloneGroups;

    static std::string const tableTokens;

    static std::string const tableTokenTexts;

    static std::ofstream sourcererOutput;

    static std::mutex m_;


};

template<TokenizerType T>
std::string const Writer<T>::tableProjects = "projects";

template<TokenizerType T>
std::string const Writer<T>::tableFiles = Writer<T>::tablePrefix() + "files";

template<TokenizerType T>
std::string const Writer<T>::tableStats = Writer<T>::tablePrefix() + "stats";

template<TokenizerType T>
std::string const Writer<T>::tableClonePairs = Writer<T>::tablePrefix() + "tokenizer_clonePairs";

template<TokenizerType T>
std::string const Writer<T>::tableCloneGroups = Writer<T>::tablePrefix() + "cloneGroups";

template<TokenizerType T>
std::string const Writer<T>::tableTokens = Writer<T>::tablePrefix() + "tokens";

template<TokenizerType T>
std::string const Writer<T>::tableTokenTexts = Writer<T>::tablePrefix() + "tokenTexts";

template<TokenizerType T>
std::ofstream Writer<T>::sourcererOutput;

template<TokenizerType T>
std::mutex Writer<T>::m_;
