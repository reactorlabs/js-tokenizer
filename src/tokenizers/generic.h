#pragma once

#include <map>
#include <fstream>
#include <iostream>

#include "../data.h"

class BaseTokenizer {
public:
    BaseTokenizer(std::string const & file, std::shared_ptr<TokenizedFile> tf):
        file_(file),
        tf_(tf),
        tokens_(new TokensMap(tf)),
        pos_(0) {
        tf->lines = 1; // every file has at least one line
    }

    virtual void tokenize()  = 0;

    std::shared_ptr<TokensMap> tokensMap() {
        return tokens_;
    }

protected:
    bool eof() {
        return pos_ >= file_.size();
    }

    char top() {
        if (pos_ >= file_.size())
            return 0;
        if (file_[pos_] == '\r')
            return '\n';
        return file_[pos_];
    }

    void pop() {
        if (pos_ >= file_.size())
            return;
        if (file_[pos_] == '\n' and peek(1) == '\r')
            pos_ += 2;
        else if (file_[pos_] == '\r' and peek(1) == '\n')
            pos_ += 2;
        else
            ++pos_;
    }

    void pop(unsigned by) {
        if (by == 0) {
            Thread::Error("Advancing by 0 in tokenizer, updated to one");
            by = 1;
        }
        //assert(by > 0);
        while (by-- > 0)
            pop();
    }

    char peek(int offset) {
        if (pos_ + offset < 0 or pos_ + offset >= file_.size())
            return 0;
        return file_[pos_ + offset];
    }

    unsigned pos() {
        return pos_;
    }

    void newline(bool isEmpty, bool isComment) {
        //if (isEmpty)
        //    std::cout << tf_->lines << " " << isEmpty << " " << isComment << std::endl;
        ++tf_->lines;
        if (not isEmpty)
            ++tf_->sloc;
        if (not isComment and not isEmpty)
            ++tf_->loc;
    }

    void addToken(unsigned start, unsigned length) {
        if (length > 0) {
            ++tokens()[file_.substr(start, length)];
            ++tf_->totalTokens;
        }
    }

    std::string substr(unsigned start, unsigned length) {
        if (length == 0) {
            Thread::Error("Empty substring calculated in tokenizer");
            // assert(false);
            return "";
        }
        return file_.substr(start, length);
    }

    TokenizedFile & tf() {
        return * tf_;
    }

    TokensMap & tokens() {
        return * tokens_;
    }

private:
    std::string const & file_;
    std::shared_ptr<TokenizedFile> tf_;
    std::shared_ptr<TokensMap> tokens_;
    unsigned pos_;
};

class GenericTokenizer : public BaseTokenizer {
public:
    static TokenizerKind const kind = TokenizerKind::Generic;

    GenericTokenizer(std::string const & file, std::shared_ptr<TokenizedFile> tf):
        BaseTokenizer(file, tf) {
        tf->tokenizer = TokenizerKind::Generic;
    }

    static unsigned ErrorFiles() {
        return errorFiles_;
    }

    void tokenize() override {
        hasComment_ = false;
        hasToken_ = false;
        unsigned start = 0;
        unsigned tokenLength_ = 0;
        char c ;
        while (not eof()) {
            c = top();
            if (top() == '/') {
                // single line comment
                if (peek(1) == '/') {
                    pop(2);
                    hasComment_ = true;
                    while (not eof() and top() != '\n')
                        pop();
                // multi-line comment
                } else if (peek(1) == '*') {
                    pop(2);
                    hasComment_ = true;
                    while (not eof()) {
                        if (top() == '\n') {
                            newline();
                            hasComment_ = true;
                        }
                        else if (top() == '*' and peek(1) == '/') {
                            pop(2);
                            break;
                        }
                        pop();
                    }
                }
            }
            // the comment might have been the last thing
            if (eof())
                break;
            switch (top()) {
                case ';':
                case '.':
                case '[':
                case ']':
                case '(':
                case ')':
                case '~':
                case '!':
                case '-':
                case '+':
                case '&':
                case '*':
                case '/':
                case '%':
                case '<':
                case '>':
                case '^':
                case '|':
                case '?':
                case '{':
                case '}':
                case '=':
                case '#':
                case ',':
                case '"':
                case '\\':
                case ':':
                case '$':
                case '\'':
                    hasToken_ = true; // separators are people too!
                case '\t':
                case ' ':
                case '\r':
                case '\n':
                    addToken(start, tokenLength_);
                    if (top() == '\n')
                        newline();
                    pop();
                    start = pos();
                    tokenLength_ = 0;
                    break;
                default:
                    pop();
                    ++tokenLength_;
                    break;
            }
        }
        addToken(start, tokenLength_);
        // this is to account for the last line, if it is not empty it will be wrongly calculated
        if (hasToken_ or hasComment_) {
            newline();
            --tf().lines;
        }
        // increase error counter if there were JS error files
        if (tf().errors > 0)
            ++errorFiles_;
    }

private:

    void newline() {
        BaseTokenizer::newline(not (hasComment_ or hasToken_), hasComment_ and not hasToken_);
        hasComment_ = false;
        hasToken_ = false;
    }

    void addToken(unsigned start, unsigned length) {
        if (length > 0) {
            BaseTokenizer::addToken(start, length);
            hasToken_ = true;
        }
    }

private:
    bool hasComment_;
    bool hasToken_;

    static std::atomic_uint errorFiles_;


};
