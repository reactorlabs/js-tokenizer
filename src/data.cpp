#include "data.h"
#include "workers/DBWriter.h"
#include "workers/Writer.h"

bool ClonedProject::keepProjects_ = false;

unsigned ClonedProject::stride_count_ = 1;
unsigned ClonedProject::stride_index_ = 0;



std::atomic_uint TokenizedFile::idCounter_(0);
std::atomic_uint TokenInfo::idCounter_(0);



void Buffer::guardedFlush() {
    if (target_ == Target::DB) {
        std::string tableName = DBWriter::TableName(tokenizer_, kind_);
        switch (kind_) {
            case Kind::Stats:
                buffer_ = STR("INSERT IGNORE INTO " << tableName << " VALUES " << buffer_);
                break;
            default:
                buffer_ = STR("INSERT INTO " << tableName << " VALUES " << buffer_);
        }
        DBWriter::Schedule(DBWriterJob(std::move(buffer_)));
    } else {
        Writer::Schedule(WriterJob(kind_, tokenizer_, std::move(buffer_)));
    }
    assert(buffer_.empty());
}




