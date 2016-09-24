#pragma once

#include <map>
#include <fstream>
#include <cassert>
#include <iostream>

#include "hashes/md5.h"

#include "utils.h"

#include "data.h"


class Tokenizer {
public:
    /** Tokenizes the given file and updates its information.
     */
    static void tokenize(TokenizedFile * f) {
        Tokenizer t(f);
        t.tokenize();
        f->calculateTokensHash();
        f->fileHash_ = t.fileHash_.getHash();
    }

    /** Initializes the tokenizer's separator matching FSM.
     */
    static void initializeLanguage();


private:

    enum class Kind {
#define GENERATE_COMMENT_KIND(START, END, NAME) NAME ## _start, NAME ## _end,
#define GENERATE_LITERAL_KIND(START, ESCAPE, NAME) NAME ##_start, NAME ## _escape,
        COMMENTS(GENERATE_COMMENT_KIND)
        LITERALS(GENERATE_LITERAL_KIND)
        none,
    };

    static bool isCommentKind(Kind k) {
        // no need to deal with ends
#define IS_COMMENT(START, END, NAME) if (k == Kind::NAME ##_start) return true;
        COMMENTS(IS_COMMENT)
        return false;
    }

    static bool isLiteralKind(Kind k) {
        // no need to deal with ends
#define IS_LITERAL(START, ESCAPE, NAME) if (k == Kind::NAME ##_start) return true;
        LITERALS(IS_LITERAL)
        return false;
    }

    /** Returns the corresponding end kind for comments or literals. */
    static Kind kindEndFor(Kind k) {
#define GENERATE_COMMENT_END_FOR(START, END, NAME) if (k == Kind::NAME ## _start) return Kind::NAME ## _end;
#define GENERATE_LITERAL_END_FOR(START, ESCAPE, NAME) if (k == Kind::NAME ## _start) return Kind::NAME ## _start;
        COMMENTS(GENERATE_COMMENT_END_FOR)
        LITERALS(GENERATE_LITERAL_END_FOR)
        return Kind::none;
    }

    /** Returns token that serves as an escape character for given literal kind, or the token itself so that comparison against Kind::none can be done.

      This is ok because literal token can never be its own escape.
     */
    static Kind escapeFor(Kind k) {
#define GENERATE_LITERAL_ESCAPE_FOR(START, ESCAPE, NAME) if (k == Kind::NAME ## _start) return Kind::NAME ## _escape;
        LITERALS(GENERATE_LITERAL_ESCAPE_FOR)
        return k;
    }

    class MatchStep;

    Tokenizer(TokenizedFile * f):
        f_(*f),
        file_(f->absPath()) {
        if (not file_.good())
            throw STR("Unable to open file " << f->absPath());
    }

    /** Returns true if tokenize is inside a comment
     */
    bool inComment() const {
#define GENERATE_COMMENT_END_CHECK(START, END, NAME) if (commentEnd_ == Kind::NAME ## _end) return true;
        COMMENTS(GENERATE_COMMENT_END_CHECK)
        return false;
    }

    /** Returns true if tokenize is inside a literal
     */
    bool inLiteral() const {
#define GENERATE_LITERAL_END_CHECK(START, ESCAPE, NAME) if (commentEnd_ == Kind::NAME ## _start) return true;
        LITERALS(GENERATE_LITERAL_END_CHECK)
        return false;
    }

    /** Reads new character from the file. Converts line endings to `\n` where required.
     */
    char get();

    /** Tokenizes the entire file.
     */
    void tokenize();

    /** Processes given match in the temporary string and updates the tokens and the string accordingly.
     */
    void processMatch(MatchStep * match, unsigned matchPos, std::string & temp);

    /** Adds given token to the list of tokens.

      If the tokenizer is currently inside comments, adds the token's size to commentBytes and does not add it to the tokens.
     */
    void addToken(std::string const & token);




    static MatchStep * initialState_;

    TokenizedFile & f_;
    std::ifstream file_;

    MD5 fileHash_;

    bool inComment_;

    /** Either CommentKind::invalid if tokenization is not inside a comment, or comment kind to end the comment if inside comment.
     */
    Kind commentEnd_;

    /** True if there was only whitespace in the last line.
     */
    bool emptyLine_;

    /** True if the line contains only comments.
     */
    bool commentLine_;
};
