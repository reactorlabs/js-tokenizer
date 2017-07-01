#include "buffers.h"
#include "workers/Writer.h"

std::unordered_map<Buffer::ID, Buffer *> Buffer::buffers_;


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

void Buffer::FlushAll() {
    for (auto i : buffers_)
        i.second->flush();
}

Buffer::Buffer(Kind kind, TokenizerKind tokenizer):
    kind_(kind),
    tokenizer_(tokenizer) {
}

void Buffer::guardedFlush() {
    Writer::Schedule(WriterJob(FileName(kind_, tokenizer_, STR(ClonedProject::StrideIndex())), std::move(buffer_)));
    assert(buffer_.empty());
}

std::string Buffer::FileName(Buffer::Kind kind, TokenizerKind tokenizer, std::string const & stride) {
    switch (kind) {
        case Kind::Stamp:
        case Kind::Summary:
            if (stride.empty())
                return STR(kind << ".txt");
            else
                return STR(kind << "_" << stride << ".txt");
        case Kind::Error:
            assert(false and "This should never happen");
        default:
            if (stride.empty())
                return STR(kind << ".csv");
            else
                return STR(kind << "_" << stride << ".csv");
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
            s << "clone_pairs";
            break;
        case Buffer::Kind::CloneGroups:
            s << "clone_groups";
            break;
        case Buffer::Kind::Tokens:
            s << "tokens";
            break;
        case Buffer::Kind::TokensText:
            s << "tokens_text";
            break;
        case Buffer::Kind::TokenizedFiles:
            s << "tokenized_files";
            break;
        case Buffer::Kind::Summary:
            s << "summary";
            break;
        default:
            s << "Error";
            break;
    }
    return s;
}


