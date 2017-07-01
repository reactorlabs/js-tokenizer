#pragma once

#include "data.h"

/** Tokenizer uses buffer for any of its outputs. Each buffer targets either a database, or a file.

 */

class Buffer {
public:
    enum class Kind {
        Projects,
        ProjectsExtra,
        Files,
        FilesExtra,
        Stats,
        ClonePairs,
        CloneGroups,
        Tokens,
        TokensText,
        TokenizedFiles,
        Stamp,
        Summary,
        Error
    };

    struct ID {
        Kind kind;
        TokenizerKind tokenizer;
        ID(Kind kind, TokenizerKind tokenizer = TokenizerKind::Generic):
            kind(kind),
            tokenizer(tokenizer) {
        }
        bool operator == (ID const & other) const {
            return kind == other.kind and tokenizer == other.tokenizer;
        }
    };

    static const unsigned BUFFER_LIMIT = 1024 * 1024; //1mb

    Buffer(Kind kind, TokenizerKind tokenizer = TokenizerKind::Generic);

    void append(std::string const & what) {
        std::lock_guard<std::mutex> g(m_);
        buffer_ += STR(what << std::endl);
        if (buffer_.size() > BUFFER_LIMIT)
            guardedFlush();
    }

    void flush() {
        std::lock_guard<std::mutex> g(m_);
        if (not buffer_.empty())
            guardedFlush();
    }

    static Buffer & Get(Kind kind);

    static Buffer & Get(Kind kind, TokenizerKind tokenizer);


    static void FlushAll();

    std::string static FileName(Buffer::Kind kind, TokenizerKind tokenizer, std::string const & stride);

private:

    void guardedFlush();

    Kind kind_;
    TokenizerKind tokenizer_;
    std::string buffer_;
    std::mutex m_;

    static std::unordered_map<ID, Buffer *> buffers_;

};

namespace std {

template<>
struct hash<::Buffer::ID> {
    std::size_t operator()(::Buffer::ID const & h) const {
        return (int)h.tokenizer * 100 + (int) h.kind;
    }
};

template<>
struct hash<::Buffer::Kind> {
    std::size_t operator()(::Buffer::Kind const & h) const {
        return (int) h;
    }
};
}





std::ostream & operator << (std::ostream & s, Buffer::Kind k);

