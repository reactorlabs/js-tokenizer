#include <fstream>

#include "js.h"

#include "../worker.h"

namespace {

    std::unordered_set<std::string> initializeJSKeywords() {
        std::unordered_set<std::string> result;
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

std::unordered_set<std::string> JavaScriptTokenizer::jsKeywords_ = initializeJSKeywords();

bool JavaScriptTokenizer::ignoreComments_ = true;
bool JavaScriptTokenizer::ignoreSeparators_ = true;
bool JavaScriptTokenizer::ignoreWhitespace_ = true;
std::atomic_uint JavaScriptTokenizer::errorFiles_(0);


bool JavaScriptTokenizer::isKeyword(std::string const &s) {
    return jsKeywords_.find(s) != jsKeywords_.end();
}

void JavaScriptTokenizer::addToken(std::string const & s) {
/*    if (s.size() > 1000) {
        std::cout << "Token: " << s << std::endl;
        std::cout << f_.absPath() << std::endl;
        //exit(1);
    } */
    ++tokens()[s];
    commentLine_ = false;
}

void JavaScriptTokenizer::addToken(size_t start) {
    addToken(substr(start, pos() - start));
}

void JavaScriptTokenizer::addSeparator(size_t start) {
    tf().separatorBytes += pos() - start;
    commentLine_ = false;
    if (not ignoreSeparators_)
        addToken(start);
}

void JavaScriptTokenizer::addComment(size_t start) {
    tf().commentBytes += pos() - start;
    emptyLine_ = false;
    if (not ignoreComments_)
        addToken(start);
}

void JavaScriptTokenizer::addWhitespace(size_t start) {
    tf().whitespaceBytes += pos() - start;
    if (not ignoreWhitespace_)
        addToken(start);
}


void JavaScriptTokenizer::newline() {
    BaseTokenizer::newline(emptyLine_, commentLine_);
    commentLine_ = true;
    emptyLine_ = true;
}

void JavaScriptTokenizer::numericLiteral() {
    size_t start = pos();
    // check if it is hex number
    if (top() == '0') {
        if (peek(1) == 'x' or peek(1) == 'X') {
            pop(2);
            while(not eof() and isHexDigit(top()))
                pop(1);
            addToken(start);
            return;
        } else if (peek(1) == 'o' or peek(1) == 'O') {
            pop(2);
            while(not eof() and isOctDigit(top()))
                pop(1);
            addToken(start);
            return;
        } else if (peek(1) == 'b' or peek(1) == 'B') {
            pop(2);
            while(not eof() and isBinDigit(top()))
                pop(1);
            addToken(start);
            return;
        }
    }
    while (not eof() and isDecDigit(top()))
        pop(1);
    numericLiteralFloatingPointPart();
    numericLiteralExponentPart();
    addToken(start);
}


void JavaScriptTokenizer::numericLiteralFloatingPointPart() {
    if (top() == '.') {
        pop(1);
        while (not eof() and isDecDigit(top()))
            pop(1);
    }
}

void JavaScriptTokenizer::numericLiteralExponentPart() {
    if (top() == 'e' or top() == 'E') {
        pop(1);
        if (top() == '-' or top() == '+')
            pop(1);
        while (not eof() and isDecDigit(top()))
            pop(1);
    }
}

void JavaScriptTokenizer::templateLiteral() {
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
        Thread::Log("Unterminated template literal");
        ++tf().errors;
    }
    addToken(start);
}


void JavaScriptTokenizer::stringLiteral() {
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
            if (eof())
                break;
        }
        if (top() == '\n') {
            Thread::Log("Multiple lines in string literal");
            ++tf().errors;
            addToken(start);
            return;
        }
        pop(1);
    }
    if (not eof()) {
        pop(1); // delimiter
    } else {
        Thread::Log("Unterminated string literal");
        ++tf().errors;
    }
    addToken(start);
}

void JavaScriptTokenizer::regularExpressionLiteral() {
    unsigned start = pos();
    pop(1);
    while (not eof() and top() != '/') {
        // character sets may contain unescaped characters, so we have to handle them separately
        if (top() == '[') {
            pop(1);
            while (top() != ']') {
                if (eof()) {
                    Thread::Log("Missing character set end in regular expression, unterminated regular expression");
                    ++tf().errors;
                    addToken(start);
                    return;
                }
                pop(1);

            }
        }
        if (top() == '\\') { // any escape will do
            if (eof()) {
                Thread::Log("Missing escape in regular expression, unterminated regular expression");
                ++tf().errors;
                addToken(start);
                return;
            }
            pop(1);
        }
        if (top() == '\n') {
            Thread::Log("Multiple lines in regular expression literal");
            ++tf().errors;
            addToken(start);
            return;
        }
        pop(1);

    }
    if (not eof()) {
        pop(1); // delimiter
    } else {
        Thread::Log("Unterminated regular expression literal");
        ++tf().errors;
    }
    // now parse the flags, as if identifier
    while (isIdentifier(top()))
        pop(1);
    addToken(start);
}


bool JavaScriptTokenizer::identifierOrKeyword() {
    unsigned start = pos();
    while (isIdentifier(top()))
        pop(1);
    if (pos() == start) {
        Thread::Log("Unknown character");
        ++tf().errors;
        return false;
    } else {
        std::string s = substr(start, pos() - start);
        addToken(s);
        return isKeyword(s);
    }
}

void JavaScriptTokenizer::singleLineComment() {
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

void JavaScriptTokenizer::multiLineComment() {
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
        Thread::Log("Unterminated multi-line comment");
        ++tf().errors;
    }
    addComment(start);
}

void JavaScriptTokenizer::tokenize() {
    emptyLine_ = true;
    commentLine_ = true;
    bool expectRegExp = true;
    size_t start = 1;
    while (not eof()) {
        if (start == pos()) {
            Thread::Log(STR("Unknown character " << top()));
            ++tf().errors;
            pop(1);
            addToken(substr(start, pos() - start));
        }
        start = pos();
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
    // increase error counter if there were JS error files
    if (tf().errors > 0)
        ++errorFiles_;
}
