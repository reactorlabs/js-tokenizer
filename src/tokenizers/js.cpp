#include <fstream>

#include "../hashes/md5.h"
#include "js.h"

#include "../old/worker.h"

namespace {

    std::set<std::string> initializeJSKeywords() {
        std::set<std::string> result;
        result.insert("abstract");
        result.insert("arguments");
        result.insert("boolean");
        result.insert("break");
        result.insert("byte");
        result.insert("case");
        result.insert("catch");
        result.insert("char");
        result.insert("class");
        result.insert("const");
        result.insert("continue");
        result.insert("debugger");
        result.insert("default");
        result.insert("delete");
        result.insert("do");
        result.insert("double");
        result.insert("else");
        result.insert("enum");
        result.insert("evail");
        result.insert("export");
        result.insert("extends");
        result.insert("false");
        result.insert("final");
        result.insert("finally");
        result.insert("float");
        result.insert("for");
        result.insert("function");
        result.insert("goto");
        result.insert("if");
        result.insert("implements");
        result.insert("import");
        result.insert("in");
        result.insert("instanceof");
        result.insert("int");
        result.insert("interface");
        result.insert("let");
        result.insert("long");
        result.insert("native");
        result.insert("new");
        result.insert("null");
        result.insert("package");
        result.insert("private");
        result.insert("protected");
        result.insert("public");
        result.insert("return");
        result.insert("short");
        result.insert("static");
        result.insert("super");
        result.insert("switch");
        result.insert("synchronized");
        result.insert("this");
        result.insert("throw");
        result.insert("throws");
        result.insert("transient");
        result.insert("true");
        result.insert("try");
        result.insert("typeof");
        result.insert("var");
        result.insert("void");
        result.insert("volatile");
        result.insert("while");
        result.insert("with");
        result.insert("yield");
        return result;
    }
}

std::set<std::string> JSTokenizer::jsKeywords_ = initializeJSKeywords();


bool JSTokenizer::isKeyword(std::string const &s) {
    return jsKeywords_.find(s) != jsKeywords_.end();
}


size_t JSTokenizer::size() {
    return data_.size();
}

size_t JSTokenizer::pos() {
    return pos_;
}

bool JSTokenizer::eof() {
    return pos() == size();
}

char JSTokenizer::top() {
    if (pos_ == data_.size())
        return 0;
    return data_[pos_];
}

void JSTokenizer::pop(unsigned howMuch) {
    pos_ += howMuch;
}

char JSTokenizer::peek(int offset) {
    if (pos_ + offset == data_.size())
        return 0;
    return data_[pos_ + offset];
}

std::string JSTokenizer::substr(size_t start, size_t end) {
    try {
        return data_.substr(start, end - start);
    } catch (...) {
        std::cout << "HERE I FAIL: " << f_.absPath() << std::endl;
        std::cout << "ouch" << std::endl;
        //exit(-1);
        //system(STR("mv " << f_.absPath() << " " << "/home/peta/sourcerer/data/errors").c_str());
        throw "continuing";

    }
    return "";
}

void JSTokenizer::updateFileHash() {
    MD5 md5;
    md5.add(data_.c_str(), data_.size());
    f_.fileHash_ = md5.getHash();
}

void JSTokenizer::addToken(std::string const & s) {
/*    if (s.size() > 1000) {
        std::cout << "Token: " << s << std::endl;
        std::cout << f_.absPath() << std::endl;
        //exit(1);
    } */
    ++f_.tokens_[s];
    f_.tokenBytes_ += s.size();
    ++f_.totalTokens_;
    commentLine_ = false;
}

void JSTokenizer::addToken(size_t start) {
    addToken(substr(start, pos()));
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
    if (top() == '0') {
        if (peek(1) == 'x' or peek(1) == 'X') {
            while(not eof() and isHexDigit(top()))
                pop(1);
            addToken(start);
        } else if (peek(1) == 'o' or peek(1) == 'O') {
            while(not eof() and isOctDigit(top()))
                pop(1);
            addToken(start);
        } else if (peek(1) == 'b' or peek(1) == 'B') {
            while(not eof() and isBinDigit(top()))
                pop(1);
            addToken(start);
        }
    }
    while (not eof() and isDecDigit(top()))
        pop(1);
    numericLiteralFloatingPointPart();
    numericLiteralExponentPart();
    addToken(start);
}


void JSTokenizer::numericLiteralFloatingPointPart() {
    if (top() == '.') {
        pop(1);
        while (not eof() and isDecDigit(top()))
            pop(1);
    }
}

void JSTokenizer::numericLiteralExponentPart() {
    if (top() == 'e' or top() == 'E') {
        pop(1);
        if (top() == '-' or top() == '+')
            pop(1);
        while (not eof() and isDecDigit(top()))
            pop(1);
    }
}

void JSTokenizer::templateLiteral() {
    unsigned start = pos();
    pop(1);
    while (not eof() and top() != '`') {
        if (top() == '\\')
            pop(1);
        else if (top() == '\n')
            newline();
        pop(1);
    }
    if (not eof()) {
        pop(1); // delimiter
    } else {
        Worker::log(STR(f_.absPath() << ": Unterminated template literal"));
        ++f_.errors_;
    }
    addToken(start);
}


void JSTokenizer::stringLiteral() {
    unsigned start = pos();
    char delimiter = top();
    pop(1);
    while (not eof() and top() != delimiter) {
        if (top() == '\\') {
            pop(1);
            if (top() == '\n') {
                newline();
                pop(1);
                continue;
            }
        }
        if (top() == '\n') {
            Worker::log(STR(f_.absPath() << ": Multiple lines in string literal"));
            ++f_.errors_;
            addToken(start);
            return;
        }
        pop(1);
    }
    if (not eof()) {
        pop(1); // delimiter
    } else {
        Worker::log(STR(f_.absPath() << ": Unterminated string literal"));
        ++f_.errors_;
    }
    addToken(start);
}

void JSTokenizer::regularExpressionLiteral() {
    unsigned start = pos();
    pop(1);
    while (not eof() and top() != '/') {
        // character sets may contain unescaped characters, so we have to handle them separately
        if (top() == '[') {
            pop(1);
            while (top() != ']')
                pop(1);
        }
        if (top() == '\\') // any escape will do
            pop(1);
        if (top() == '\n') {
            Worker::log(STR(f_.absPath() << ": Multiple lines in regular expression literal"));
            ++f_.errors_;
            addToken(start);
            return;
        }
        pop(1);

    }
    if (not eof()) {
        pop(1); // delimiter
    } else {
        Worker::log(STR(f_.absPath() << ": Unterminated regular expression literal"));
        ++f_.errors_;
    }
    // now parse the flags, as if identifier
    while (isIdentifier(top()))
        pop(1);
    addToken(start);
}


bool JSTokenizer::identifierOrKeyword() {
    unsigned start = pos();
    while (isIdentifier(top()))
        pop(1);
    std::string s = substr(start, pos());
    addToken(s);
    return isKeyword(s);
}

void JSTokenizer::singleLineComment() {
    unsigned start = pos();
    emptyLine_ = false; // the line definitely contains at least the comment
    pop(2); // //
    while (not eof() and top() != '\n')
        pop(1);
    newline();
    if (not eof())
        pop(1);
    addComment(start);
}

void JSTokenizer::multiLineComment() {
    unsigned start = pos();
    emptyLine_ = false; // the line definitely contains at least the comment
    pop(2); // /*
    while (not eof()) {
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
    if (eof()) {
        Worker::log(STR(f_.absPath() << ": Unterminated multi-line comment"));
        ++f_.errors_;
    }
    addComment(start);
}

void JSTokenizer::tokenize() {
    loadEntireFile();
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
                continue; // do not change regexp expectations
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
            case '`': // backticks template literal
                templateLiteral();
                expectRegExp = false;
                continue; // i.e. expect regexp false
            case '"':
            case '\'':
                stringLiteral();
                expectRegExp = false;
                continue; // i.e. expect regexp false
            case '/':
                if (peek(1) == '/') {
                    singleLineComment();
                } else if (peek(1) == '*') {
                    multiLineComment();
                } else if (expectRegExp) {
                    regularExpressionLiteral();
                    expectRegExp = false;
                    continue; // i.e. expect regexp false
                } else if (peek(1) == '=') {
                    pop(2);
                    addSeparator(start);
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
                } else if (peek(1) == '/') {
                    pop(2); // </ -- this is hack to somehow parse E4X
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
                if (isDecDigit(peek(1))) {
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
                expectRegExp = identifierOrKeyword();
                continue; // i.e. expect regexp as set above
        }
        expectRegExp = true;
    }
}











void JSTokenizer::encodeUTF8(unsigned codepoint, std::string & into) {
    if (codepoint < 0x80) {
        into += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        into += static_cast<char>(0xc0 + (codepoint >> 6));
        into += static_cast<char>(0x80 + (codepoint & 0x3f));
    } else if (codepoint < 0x10000) {
        into += static_cast<char>(0xe0 + (codepoint >> 12));
        into += static_cast<char>(0x80 + ((codepoint >> 6) & 0x3f));
        into += static_cast<char>(0x80 + (codepoint & 0x3f));
    } else if (codepoint < 0x200000) {
        into += static_cast<char>(0xf0 + (codepoint >> 18));
        into += static_cast<char>(0x80 + ((codepoint >> 12) & 0x3f));
        into += static_cast<char>(0x80 + ((codepoint >> 6) & 0x3f));
        into += static_cast<char>(0x80 + (codepoint & 0x3f));
    } else if (codepoint < 0x4000000) {
        into += static_cast<char>(0xf8 + (codepoint >> 24));
        into += static_cast<char>(0x80 + ((codepoint >> 18) & 0x3f));
        into += static_cast<char>(0x80 + ((codepoint >> 12) & 0x3f));
        into += static_cast<char>(0x80 + ((codepoint >> 6) & 0x3f));
        into += static_cast<char>(0x80 + (codepoint & 0x3f));
    } else {
        if (codepoint >= 0x80000000)
            ++f_.errors_;
        into += static_cast<char>(0xfc + (codepoint >> 30));
        into += static_cast<char>(0x80 + ((codepoint >> 24) & 0x3f));
        into += static_cast<char>(0x80 + ((codepoint >> 18) & 0x3f));
        into += static_cast<char>(0x80 + ((codepoint >> 12) & 0x3f));
        into += static_cast<char>(0x80 + ((codepoint >> 6) & 0x3f));
        into += static_cast<char>(0x80 + (codepoint & 0x3f));
    }


}


void JSTokenizer::convertUTF16be() {
    Worker::log(STR(f_.absPath() << ": Converting from UTF16be"));
    std::string result;
    result.reserve(data_.size());
    size_t i = 2, e = data_.size();
    while (i < e) {
        unsigned cp = ((unsigned)data_[i] << 8) + (unsigned)data_[i+1];
        i += 2;
        // this might be 2x codepoint
        if (cp >= 0xd800) {
            unsigned cp2 = ((unsigned)data_[i] << 8) + (unsigned)data_[i+1];
            if (cp2 >= 0xdc00) {
                i += 2;
                cp = 0x10000 + ((cp = 0xd800) << 10) + (cp2 - 0xdc00);
            }
        }
        encodeUTF8(cp, result);
    }
    data_ = std::move(result);
}



void JSTokenizer::convertUTF16le() {
    Worker::log(STR(f_.absPath() << ": Converting from UTF16le"));
    std::string result;
    result.reserve(data_.size());
    size_t i = 2, e = data_.size();
    while (i < e) {
        unsigned cp = ((unsigned)data_[i + 1] << 8) + (unsigned)data_[i];
        i += 2;
        // this might be 2x codepoint
        if (cp >= 0xd800) {
            unsigned cp2 = ((unsigned)data_[i + 1] << 8) + (unsigned)data_[i];
            if (cp2 >= 0xdc00) {
                i += 2;
                cp = 0x10000 + ((cp = 0xd800) << 10) + (cp2 - 0xdc00);
            }
        }
        encodeUTF8(cp, result);
    }
    data_ = std::move(result);
}




void JSTokenizer::loadEntireFile() {
    std::ifstream s(f_.absPath(), std::ios::in | std::ios::binary);
    if (not s.good())
        throw STR("Unable to open file " << f_.absPath());
    s.seekg(0, std::ios::end);
    data_.resize(s.tellg());
    s.seekg(0, std::ios::beg);
    s.read(& data_[0], data_.size());
    s.close();
    pos_ = 0;
    // check the encoding BOMs
    if (data_.size() >= 2) {
        if ((data_[0] == (char) 0xff and data_[1] == (char)0xfe)) {
            convertUTF16le();
        } else if (data_[0] == (char) 0xfe and data_[1] == (char)0xff) {
            convertUTF16be();
        }
    }
    // UTF-8, skip if present
    if (data_.size() >= 3 and data_[0] == (char)0xef and data_[1] == (char) 0xbb and data_[2] == (char) 0xbf)
            pos_ = 3;
    f_.bytes_ = data_.size();
}
