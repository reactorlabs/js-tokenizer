#include <fstream>

#include "../hashes/md5.h"
#include "js.h"


size_t JSTokenizer::size() {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    return data_.size();
#endif
}

size_t JSTokenizer::pos() {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    return pos_;
#endif
}

bool JSTokenizer::eof() {
    return pos() == size();
}

char JSTokenizer::top() {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    if (pos_ == data_.size())
        return 0;
    return data_[pos_];
#endif
}

void JSTokenizer::pop(unsigned howMuch) {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    pos_ += howMuch;
#endif
}

char JSTokenizer::peek(int offset) {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    if (pos_ + offset == data_.size())
        return 0;
    return data_[pos_ + offset];
#endif
}

std::string JSTokenizer::substr(size_t start, size_t end) {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    return data_.substr(start, end - start);
#endif
}

void JSTokenizer::updateFileHash() {
    MD5 md5;
    md5.add(data_.c_str(), data_.size());
    f_.fileHash_ = md5.getHash();
}

void JSTokenizer::addToken(size_t start) {
    //std::cout << "Token: " << substr(start, pos()) << std::endl;
    ++f_.tokens_[substr(start, pos_)];
    f_.tokenBytes_ += pos_ - start;
    ++f_.totalTokens_;
    commentLine_ = false;
}

void JSTokenizer::addSeparator(size_t start) {
    //std::cout << "Separator: " << substr(start, pos()) << std::endl;
    f_.separatorBytes_ += pos_ - start;
    commentLine_ = false;
}

void JSTokenizer::addComment(size_t start) {
    //std::cout << "Comment: " << substr(start, pos()) << std::endl;
    f_.commentBytes_ += pos_ - start;
    emptyLine_ = false;
}

void JSTokenizer::addWhitespace(size_t start) {
    //std::cout << "Whitespace: " << substr(start, pos()) << std::endl;
    f_.whitespaceBytes_ += pos_ - start;
}


void JSTokenizer::newline() {
    ++f_.loc_;
    if (commentLine_ == true) {
        if (emptyLine_ == true)
            ++f_.emptyLoc_;
        else
            ++f_.commentLoc_;
   }
    commentLine_ = true;
    emptyLine_ = true;
}

void JSTokenizer::numericLiteral() {
    size_t start = pos();
    // check if it is hex number
    if (top() == '0' and (peek(1) == 'x' or peek(1) == 'X')) {
        while(not eof() and isHexDigit(top()))
            pop(1);
    } else {
        while (not eof() and isDecimalDigit(top()))
            pop(1);
        numericLiteralFloatingPointPart();
        numericLiteralExponentPart();
    }
    addToken(start);
}


void JSTokenizer::numericLiteralFloatingPointPart() {
    if (top() == '.') {
        pop(1);
        while (not eof() and isDecimalDigit(top()))
            pop(1);
    }
}

void JSTokenizer::numericLiteralExponentPart() {
    if (top() == 'e' or top() == 'E') {
        pop(1);
        if (top() == '-' or top() == '+')
            pop(1);
        while (not eof() and isDecimalDigit(top()))
            pop(1);
    }
}


void JSTokenizer::stringLiteral() {
    unsigned start = pos();
    char delimiter = top();
    pop(1);
    while (not eof() and top() != delimiter) {
        if (top() == '\\' and peek(1) == delimiter)
            pop(1);
        else if (top() == '\n')
            newline();
        pop(1);
    }
    pop(1); // delimiter
    addToken(start);
}

void JSTokenizer::regularExpressionLiteral() {
    unsigned start = pos();
    pop(1);
    while (not eof() and top() != '/')
        pop(1);
    // now parse the flags, as if identifier
    while (isIdentifier(top()))
        pop(1);
    addToken(start);
}

void JSTokenizer::identifierOrKeyword() {
    unsigned start = pos();
    while (isIdentifier(top()))
        pop(1);
    addToken(start);
}

void JSTokenizer::singleLineComment() {
    unsigned start = pos();
    emptyLine_ = false; // the line definitely contains at least the comment
    pop(2); // //
    while (top() != '\n')
        pop(1);
    newline();
    pop(1);
    addComment(start);
}

void JSTokenizer::multiLineComment() {
    unsigned start = pos();
    emptyLine_ = false; // the line definitely contains at least the comment
    pop(2); // /*
    while (true) {
        if (top() == '*' and peek(1) == '/') {
            pop(2);
            break;
        } else {
            if (top() == '\n') {
                newline();
                emptyLine_ = false; // the line definitely contains at least the comment
            }
            pop(1);
        }
    }
    addComment(start);
}

void JSTokenizer::tokenize() {
#if JS_TOKENIZER_ENTIRE_FILE == 1
    loadEntireFile();
#endif
    emptyLine_ = true;
    commentLine_ = true;
    size_t e = size();
    bool expectRegExp = true;
    while (pos() != e) {
        size_t start = pos();
        switch (top()) {
            case '\n':
                newline();
            case ' ':
            case '\t':
            case '\r':
                pop(1);
                addWhitespace(start);
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                numericLiteral();
                expectRegExp = false;
                continue; // i.e. expect regexp false
            case '"':
            case '\'':
                stringLiteral();
                expectRegExp = false;
                continue; // i.e. expect regexp false
            case '/':
                if (peek(1) == '=') {
                    pop(2);
                    addSeparator(start);
                } else if (peek(1) == '/') {
                    singleLineComment();
                } else if (peek(1) == '*') {
                    multiLineComment();
                } else if (expectRegExp) {
                    regularExpressionLiteral();
                    expectRegExp = false;
                    continue; // i.e. expect regexp false
                } else {
                    pop(1);
                    addSeparator(start);
                }
                break;
            case '>':
                if (peek(1) == '>') {
                    if (peek(2) == '>') {
                        if (peek(3) == '=') {
                            pop(4); // >>>=
                        } else {
                            pop(3); // >>>
                        }
                    } else if (peek(2) == '=') {
                        pop(3); // >>=
                    } else {
                        pop(2); // >>
                    }
                } else if (peek(1) == '=') {
                    pop(2); // >=
                } else {
                    pop(1); // >
                }
                addSeparator(start);
                break;
            case '<':
                if (peek(1) == '<') {
                    if (peek(2) == '=') {
                        pop(3); // <<=
                    } else {
                        pop(2); // <<
                    }
                } else if (peek(1) == '=') {
                    pop(2); // <=
                } else {
                    pop(1); // <
                }
                addSeparator(start);
                break;
            case '=':
                if (peek(1) == '=') {
                    if (peek(2) == '=') {
                        pop(3); // ===
                    } else {
                        pop(2); // ==
                    }
                } else {
                    pop(1); // =
                }
                addSeparator(start);
                break;
            case '!':
                if (peek(1) == '=')
                    if (peek(2) == '=')
                        pop(3); // !==
                    else
                        pop(2); // !=
                else
                    pop(1); // !
                addSeparator(start);
                break;
            case '+':
                if (peek(1) == '+')
                    pop(2); // ++
                else if (peek(1) == '=')
                    pop(2); // +=
                else
                    pop(1); // +
                addSeparator(start);
                break;
            case '-':
                if (peek(1) == '-')
                    pop(2); // --
                else if (peek(1) == '=')
                    pop(2); // -=
                else
                    pop(1); // -
                addSeparator(start);
                break;
            case '*':
                if (peek(1) == '=')
                    pop(2); // *=
                else
                    pop(1); // *
                addSeparator(start);
                break;
            case '%':
                if (peek(1) == '=')
                    pop(2); // %=
                else
                    pop(1); // %
                addSeparator(start);
                break;
            case '&':
                if (peek(1) == '&')
                    pop(2); // &&
                else if (peek(1) == '=')
                    pop(2); // &=
                else
                    pop(1); // &
                addSeparator(start);
                break;
            case '|':
                if (peek(1) == '|')
                    pop(2); // ||
                else if (peek(1) == '=')
                    pop(2); // |=
                else
                    pop(1); // |
                addSeparator(start);
                break;
            case '^':
                if (peek(1) == '=')
                    pop(2); // ^=
                else
                    pop(1); // ^
                addSeparator(start);
                break;
            case '.':
                if (isDecimalDigit(peek(1))) {
                    numericLiteralFloatingPointPart();
                    numericLiteralExponentPart();
                    addToken(start);
                    expectRegExp = false;
                    continue; // i.e. expect regexp false
                } else {
                    pop(1);
                    addSeparator(start);
                }
                break;
            case '}':
            case ')':
            case ']':
                pop(1);
                addSeparator(start);
                expectRegExp = false;
                continue; // i.e. expect regexp false
            case '{':
            case '(':
            case '[':
            case ',':
            case ';':
            case ':':
            case '~':
            case '?':
                pop(1);
                addSeparator(start);
                break;
            default:
                // identifier or keyword?
                identifierOrKeyword();
                expectRegExp = false;
                continue; // i.e. expect regexp false
        }
        expectRegExp = true;
    }
}























#if JS_TOKENIZER_ENTIRE_FILE == 1
void JSTokenizer::loadEntireFile() {
    std::ifstream s(f_.absPath(), std::ios::in | std::ios::binary);
    s.seekg(0, std::ios::end);
    data_.resize(s.tellg());
    s.seekg(0, std::ios::beg);
    s.read(& data_[0], data_.size());
    s.close();
    pos_ = 0;
    f_.bytes_ = data_.size();
}
#endif
