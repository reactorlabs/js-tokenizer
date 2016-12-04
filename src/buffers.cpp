#include "buffers.h"
#include "workers/DBWriter.h"
#include "workers/Writer.h"

std::unordered_map<Buffer::ID, Buffer *> Buffer::buffers_;
std::unordered_map<Buffer::Kind, Buffer::Target> Buffer::bufferTargets_;


Buffer & Buffer::Get(Kind kind) {
    switch (kind) {
        case Kind::Stamp:
        case Kind::Summary:
        case Kind::Projects:
        case Kind::ProjectsExtra:
        case Kind::Files:
        case Kind::FilesExtra: {
            auto i = buffers_.find(ID(kind));
            if (i == buffers_.end())
                i = buffers_.insert(std::pair<ID, Buffer *>(ID(kind), new Buffer(kind))).first;
            return *(i->second);
        }
        default:
            assert(false and "Kind requires tokenizer specification");
    }
}

Buffer & Buffer::Get(Kind kind, TokenizerKind tokenizer) {
    switch (kind) {
        case Kind::Stats:
        case Kind::ClonePairs:
        case Kind::CloneGroups:
        case Kind::Tokens:
        case Kind::TokensText:
        case Kind::TokenizedFiles: {
            auto i = buffers_.find(ID(kind, tokenizer));
            if (i == buffers_.end())
                i = buffers_.insert(std::pair<ID, Buffer *>(ID(kind, tokenizer), new Buffer(kind, tokenizer))).first;
            return *(i->second);
        }
        default:
            assert(false and "Kind must be used without tokenizer specification");
    }
}

Buffer::Target & Buffer::TargetType(Kind kind) {
   return bufferTargets_[kind];
}

void Buffer::FlushAll() {
    for (auto i : buffers_)
        i.second->flush();
}

Buffer::Buffer(Kind kind, TokenizerKind tokenizer):
    target_(bufferTargets_[kind]),
    kind_(kind),
    tokenizer_(tokenizer) {
    if (target_ == Target::DB) {
        // if target is database, create the table
        std::stringstream ss;
        Thread::Print(STR("  creating table " << TableName(kind_, tokenizer_) << std::endl));
        ss << "CREATE TABLE IF NOT EXISTS " << TableName(kind_, tokenizer_) << "(";
        switch (kind_) {
            case Kind::Stamp:
                ss << "name VARCHAR(100) NOT NULL," <<
                      "value VARCHAR(1000) NOT NULL," <<
                      "PRIMARY KEY (name))";
                break;
            case Kind::Summary:
                ss << "name VARCHAR(100) NOT NULL," <<
                      "value BIGINT NOT NULL," <<
                      "PRIMARY KEY (name))";
                break;
            case Kind::Projects:
                ss << "projectId INT(6) NOT NULL," <<
                      "projectPath VARCHAR(4000) NULL," <<
                      "projectUrl VARCHAR(4000) NOT NULL," <<
                      "PRIMARY KEY (projectId)," <<
                      "UNIQUE INDEX (projectId))";
                break;
            case Kind::ProjectsExtra:
                ss << "projectId INT NOT NULL,"
                      "createdAt INT UNSIGNED NOT NULL,"
                      "commit CHAR(40),"
                      "PRIMARY KEY (projectId),"
                      "UNIQUE INDEX (projectId))";
                break;
            case Kind::Files:
                ss << "fileId BIGINT(6) UNSIGNED NOT NULL," <<
                      "projectId INT(6) UNSIGNED NOT NULL," <<
                      "relativeUrl VARCHAR(4000) NOT NULL," <<
                      "fileHash CHAR(32) NOT NULL," <<
                      "PRIMARY KEY (fileId)," <<
                      "UNIQUE INDEX (fileId)," <<
                      "INDEX (projectId)," <<
                      "INDEX (fileHash))";
                break;
            case Kind::FilesExtra:
                ss << "fileId INT NOT NULL," <<
                      "createdAt INT UNSIGNED NOT NULL," <<
                      "PRIMARY KEY (fileId)," <<
                      "UNIQUE INDEX (fileId))";
                break;
            case Kind::Stats:
                ss << "fileHash CHAR(32) NOT NULL," <<
                      "fileBytes INT(6) UNSIGNED NOT NULL," <<
                      "fileLines INT(6) UNSIGNED NOT NULL," <<
                      "fileLOC INT(6) UNSIGNED NOT NULL," <<
                      "fileSLOC INT(6) UNSIGNED NOT NULL," <<
                      "totalTokens INT(6) UNSIGNED NOT NULL," <<
                      "uniqueTokens INT(6) UNSIGNED NOT NULL," <<
                      "tokenHash CHAR(32) NOT NULL," <<
                      "PRIMARY KEY (fileHash)," <<
                      "UNIQUE INDEX (fileHash)," <<
                      "INDEX (tokenHash))";
                break;
            case Kind::ClonePairs:
                ss << "fileId INT NOT NULL," <<
                      "groupId INT NOT NULL," <<
                      "PRIMARY KEY(fileId)," <<
                      "UNIQUE INDEX(fileId))";
                break;
            case Kind::CloneGroups:
                ss << "groupId INT NOT NULL," <<
                      "oldestId INT NOT NULL," <<
                      "PRIMARY KEY(groupId)," <<
                      "UNIQUE INDEX(groupId))";
                break;
            case Kind::Tokens:
                ss << "id INT NOT NULL," <<
                      "uses INT NOT NULL," <<
                      "PRIMARY KEY(id)," <<
                      "UNIQUE INDEX(id))";
                break;
            case Kind::TokensText:
                ss << "id INT NOT NULL," <<
                      "size INT NOT NULL," <<
                      "hash CHAR(32)," <<
                      "text LONGTEXT NOT NULL," <<
                      "PRIMARY KEY(id)," <<
                      "UNIQUE INDEX(id))";
            case Kind::TokenizedFiles:
            default:
                assert(false and "Not supported in db mode");
        }
        DBWriter::Schedule(ss.str());
    }
}

void Buffer::guardedFlush() {
    if (target_ == Target::DB) {
        switch (kind_) {
            case Kind::Summary:
            case Kind::Stamp:
                buffer_ = STR("REPLACE INTO " << TableName(kind_, tokenizer_) << " VALUES " << buffer_);
                break;
            default:
                buffer_ = STR("INSERT INTO " << TableName(kind_, tokenizer_) << " VALUES " << buffer_);
        }
        DBWriter::Schedule(DBWriterJob(std::move(buffer_)));
    } else {
        Writer::Schedule(WriterJob(FileName(kind_, tokenizer_), std::move(buffer_)));
    }
    assert(buffer_.empty());
}

std::string Buffer::TableName(Buffer::Kind kind, TokenizerKind tokenizer) {
    switch (kind) {
        case Kind::Stamp:
        case Kind::Summary:
        case Kind::Projects:
        case Kind::ProjectsExtra:
        case Kind::Files:
        case Kind::FilesExtra:
            return STR(kind);
        case Kind::Error:
            assert(false and "This should never happen");
        default:
            return STR(tokenizer << "_" << kind);
    }
}

std::string Buffer::FileName(Buffer::Kind kind, TokenizerKind tokenizer) {
    switch (kind) {
        case Kind::Stamp:
        case Kind::Summary:
        case Kind::Projects:
        case Kind::ProjectsExtra:
        case Kind::Files:
        case Kind::FilesExtra:
            return STR(kind << ".txt");
        case Kind::Error:
            assert(false and "This should never happen");
        default:
            return STR(tokenizer << "/" << kind << ".txt");
    }
}


std::ostream & operator << (std::ostream & s, Buffer::Kind k) {
    switch (k) {
        case Buffer::Kind::Stamp:
            s << "stamp";
            break;
        case Buffer::Kind::Projects:
            s << "projects";
            break;
        case Buffer::Kind::ProjectsExtra:
            s << "projects_extra";
            break;
        case Buffer::Kind::Files:
            s << "files";
            break;
        case Buffer::Kind::FilesExtra:
            s << "files_extra";
            break;
        case Buffer::Kind::Stats:
            s << "stats";
            break;
        case Buffer::Kind::ClonePairs:
            s << "clone_pairs_" << ClonedProject::StrideIndex();
            break;
        case Buffer::Kind::CloneGroups:
            s << "clone_groups_" << ClonedProject::StrideIndex();
            break;
        case Buffer::Kind::Tokens:
            s << "tokens_" << ClonedProject::StrideIndex();
            break;
        case Buffer::Kind::TokensText:
            s << "tokens_text_" << ClonedProject::StrideIndex();
            break;
        case Buffer::Kind::TokenizedFiles:
            s << "tokenized_files_" << ClonedProject::StrideIndex();
            break;
        case Buffer::Kind::Summary:
            s << "summary_" << ClonedProject::StrideIndex();
            break;
        default:
            s << "Error";
            break;
    }


}


