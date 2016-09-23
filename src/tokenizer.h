#pragma once

#include <map>
#include <fstream>
#include <cassert>
#include <iostream>

#include "hashes/md5.h"

#include "globals.h"
#include "utils.h"
#include "records.h"

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

    enum class CommentKind {
#define GENERATE_COMMENT_KIND(START, END, NAME) NAME ## _start, NAME ## _end,
        COMMENTS(GENERATE_COMMENT_KIND)
        none,
    };

    /** Returns the corresponding end comment kind. */
    static CommentKind commentEndFor(CommentKind k) {
#define GENERATE_COMMENT_END_FOR(START, END, NAME) if (k == CommentKind::NAME ## _start) return CommentKind::NAME ## _end;
        COMMENTS(GENERATE_COMMENT_END_FOR)
        return CommentKind::none;
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
        return commentEnd_ != CommentKind::none;
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
    CommentKind commentEnd_;

    /** True if there was only whitespace in the last line.
     */
    bool emptyLine_;

    /** True if the line contains only comments.
     */
    bool commentLine_;
};
