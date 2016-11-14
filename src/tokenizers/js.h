#pragma once

#include <unordered_set>

#include "../tokenizer.h"



/** Custom built tokenizer for Javascript.
 */
class JSTokenizer {
public:
    static void tokenize(TokenizedFile * f) {
        JSTokenizer t(f);
        t.loadEntireFile();
        t.tokenize();
        //f->updateFileStats(t.data_);
    }

    static void Tokenize(TokenizedFile & f, std::string const & contents) {
        JSTokenizer t(&f);
        t.data_ = contents;
        t.pos_ = 0;
        t.tokenize();
        //f.updateFileStats(t.data_);
    }

    static bool & IgnoreSeparators() {
        return ignoreSeparators_;

    }

    static bool & IgnoreComments() {
        return ignoreComments_;
    }

    static bool & IgnoreWhitespace() {
        return ignoreWhitespace_;
    }

private:

    JSTokenizer(TokenizedFile * f):
        f_(*f) {
    }

    static bool isDecDigit(char c) {
        return c >= '0' and c <= '9';
    }

    static bool isOctDigit(char c) {
        return c >= '0' and c <= '7';
    }

    static bool isBinDigit(char c) {
        return c >= '0' and c <= '1';
    }

    static bool isHexDigit(char c) {
        return isDecDigit(c) or (c >= 'A' and c <= 'F') or (c >= 'a' and c <= 'f');
    }

    /** This works the other way around. Thanks to UTF anything is an identifier, unless it is one of the separators.
     */
    static bool isIdentifier(char c) {
       switch (c) {
           case '\n':
           case '\r':
           case ' ':
           case '\t':
           case '"':
           case '\'':
           case '`':
           case '/':
           case '>':
           case '<':
           case '=':
           case '!':
           case '+':
           case '-':
           case '*':
           case '%':
           case '&':
           case '|':
           case '^':
           case '}':
           case ')':
           case ']':
           case '{':
           case '(':
           case '[':
           case '.':
           case ',':
           case ';':
           case ':':
           case '~':
           case '?':
           case 0: // for end of file termination
               return false;
           default:
               return true;
       }
    }

    bool isKeyword(std::string const & s);

    size_t size();

    size_t pos();

    bool eof();

    char top();

    void pop(unsigned howMuch);

    char peek(int offset);

    std::string substr(size_t start, size_t end);

    void updateFileHash();



    void addToken(std::string const & s);
    void addToken(size_t start);
    void addSeparator(size_t start);
    void addComment(size_t start);
    void addWhitespace(size_t start);




    void newline();

    void numericLiteral();

    void numericLiteralFloatingPointPart();

    void numericLiteralExponentPart();


    void templateLiteral();
    /** String literal only advances, it must be appended manually afterwards.
     */
    void stringLiteral();

    void regularExpressionLiteral();

    /** Parses identifier or keyword.

      Returns true if the parsed text was keyword.
     */
    bool identifierOrKeyword();

    void singleLineComment();

    void multiLineComment();



    void tokenize();



    void encodeUTF8(unsigned codepoint, std::string & into);

    void convertUTF16le();
    void convertUTF16be();

    void loadEntireFile();

    std::string data_;
    unsigned pos_;

    TokenizedFile & f_;

    bool emptyLine_;
    bool commentLine_;



    static std::unordered_set<std::string> jsKeywords_;

    static bool ignoreComments_;
    static bool ignoreSeparators_;
    static bool ignoreWhitespace_;


};

