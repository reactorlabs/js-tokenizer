#pragma once

#include "../data.h"



/** Custom built tokenizer for Javascript.
 */
class JSTokenizer {
public:
    static void tokenize(TokenizedFile * f) {
        JSTokenizer t(f);
        t.tokenize();
        t.updateFileHash();
        f->calculateTokensHash();
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

    size_t size();

    size_t pos();

    bool eof();

    char top();

    void pop(unsigned howMuch);

    char peek(int offset);

    std::string substr(size_t start, size_t end);

    void updateFileHash();




    void addToken(size_t start);
    void addSeparator(size_t start);
    void addComment(size_t start);
    void addWhitespace(size_t start);




    void newline();

    void numericLiteral();

    void numericLiteralFloatingPointPart();

    void numericLiteralExponentPart();

    void stringLiteral();

    void regularExpressionLiteral();

    void identifierOrKeyword();

    void singleLineComment();

    void multiLineComment();



    void tokenize();






#if JS_TOKENIZER_ENTIRE_FILE == 1
    void loadEntireFile();

    std::string data_;
    unsigned pos_;
#endif


    TokenizedFile & f_;

    bool emptyLine_;
    bool commentLine_;



};

