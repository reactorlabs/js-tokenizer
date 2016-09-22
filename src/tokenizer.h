#pragma once

#include <map>
#include <fstream>
#include <cassert>
#include <iostream>

#include "hashes/md5.h"

#include "globals.h"
#include "utils.h"
#include "records.h"


class FileTokenizer {
public:

    FileTokenizer(FileRecord & r):
        record_(r) {
    }

    FileRecord & record() {
        return record_;
    }

    bool tokenize() {
       file_.open(record_.project.path + "/" + record_.path);
       if (not file_.good()) {
           #if DEBUG == 1
           std::cerr << "Unable to open file " << project.path << "/" << relPath << std::endl;
           #endif
           return false;
       }
       doTokenize();
       record_.fileHash = fileHash_.getHash();
       record_.tokensHash = calculateTokensHash();
       return true;
   }

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

    class MatchStep {
    public:

        MatchStep * next(char c) {
            auto i = next_.find(c);
            if (i == next_.end())
                return nullptr;
            return i->second;
        }

        /** Returns true if the given step is a match (i.e. final state in the automaton).
         */
        bool isMatch() {
            return length > 0;
        }

        /** Number of characters the match has, 0 if not a match
         */
        unsigned length = 0;
        /** Comment kind, if any
         */
        CommentKind kind = CommentKind::none;
        /** true if the matched string is separator.

          We need these because comments end may be whitespaces, or separators under normal circumstances.
         */
        bool isSeparator = false;
        /** true if the matched string is whitespace.

          We need these because comments end may be whitespaces, or separators under normal circumstances.
         */
        bool isWhitespace = false;

        /** Adds given match to the matching FSM.
         */
        void addMatch(std::string const & what, CommentKind kind, bool isSeparator, bool isWhitespace) {
            MatchStep * state = this;
            for (size_t i = 0, e = what.size(); i != e; ++i) {
                auto it = state->next_.find(what[i]);
                if (it == state->next_.end())
                    it = state->next_.insert(std::pair<char, MatchStep *>(what[i], new MatchStep())).first;
                state = it->second;
            }
            if (kind != CommentKind::none)
                state->kind = kind;
            if (isSeparator)
                state->isSeparator = true;
            if (isWhitespace)
                state->isWhitespace = true;
            if (state->length == 0)
                state->length = what.size();
            assert(state->length == what.size() and "Corrupted matching FSM");
        }
    private:
        std::map<char, MatchStep *> next_;
    };

    /** Returns true if tokenize is inside a comment
     */
    bool inComment() const {
        return commentEnd_ != CommentKind::none;
    }

    /** Reads new character from the file. Converts line endings to `\n` where required.
     */
    char get() {
        char result = file_.get();
        if (result == '\r' and file_.peek() == '\n') {
            result = file_.get();
        } else if (result == '\n' and file_.peek() == '\r') {
            file_.get();
        }
        if (not file_.eof()) {
            // increase total bytes (ignore different line endings
            // TODO do we want to keep / track this information ?
            ++record_.bytes;
            // add the byte to the file hash
            // TODO this ignores extra bytes in new lines, is this acceptable?
            fileHash_.add(&result, 1);
        }
        return result;
    }


    /** Calculates hash of the tokens.

      TODO we probably want to sort the hashmap before we calculate the hash
     */
    std::string calculateTokensHash() {
        // calculate hash of the tokens
        MD5 md5;
        for (auto i : record_.tokens_) {
            md5.add(i.first.c_str(), i.first.size());
            md5.add(& i.second, sizeof(i.second));
        }
        return md5.getHash();
    }

    /**




     */
    void doTokenize() {
        std::string temp;
        MatchStep * state = &initialState_;
        MatchStep * lastMatch = nullptr;
        unsigned lastMatchPos = 0;
        commentEnd_ = CommentKind::none;
        while (true) {
            char c = get();
            if (file_.eof())
                break;
            state = state->next(c);
            temp += c;
            // if the character cannot be matched
            if (state == nullptr) {
                // try matching the character from the beginning
                state = initialState_.next(c);
                // if it is not from the beginning, start from beginning for next character
                if (state == nullptr)
                    state = & initialState_;
                // if we have matched something, it is the longest one, process the match
                if (lastMatch != nullptr) {
                    addTokens(lastMatch, lastMatchPos, temp);
                    lastMatch = nullptr;
                }
            // otherwise, if we have match, remember it as last match
            }
            if (state->isMatch()) {
                lastMatch = state;
                lastMatchPos = temp.size();
            }

        }
        if (lastMatch != nullptr)
            addTokens(lastMatch, lastMatchPos, temp);
        if (not temp.empty())
            addToken(temp);
        total_bytes += record_.bytes;
    }

    void addTokens(MatchStep * match, unsigned matchPos, std::string & temp) {
        // first check if we have token
        unsigned x = matchPos - match->length;
        if (x > 0) {
            addToken(temp.substr(0, x));
        }
        // deal with new lines
        if (temp[x] == '\n') {
            if (commentLine_ and inComment())
                ++record_.commentLoc;
            else if (emptyLine_)
                ++record_.emptyLoc;
            ++record_.loc;
            emptyLine_ = true;
            commentLine_ = true;
        }
        // add the matched stuff and deal with it appropriately
        if (inComment()) {
            record_.commentBytes += match->length;
            if (match->kind == commentEnd_) {
                commentEnd_ = CommentKind::none;
            }
        // if we are not in a comment, we may start a new one
        } else {
            if (match->kind != CommentKind::none)
                commentEnd_ = commentEndFor(match->kind);
            // if we have comment end, we are inside comment
            if (inComment()) {
                record_.commentBytes += match->length;
            } else if (match->isSeparator) {
                commentLine_ = false;
                emptyLine_ = false;
            } else if (match->isWhitespace) {
                record_.whitespaceBytes += match->length;
            }
        }
        // now remove the potential token and the matched token afterwards
        temp = temp.substr(matchPos);
    }


    /** Adds given token to the list of tokens.

      If the tokenizer is currently inside comments, adds the token's size to commentBytes and does not add it to the tokens.
     */
    void addToken(std::string const & token) {
        emptyLine_ = false;
        if (inComment()) {
            record_.commentBytes += token.size();
        } else {
            record_.tokenBytes += token.size();
            ++record_.totalTokens;
            record_.tokens_[token] += 1;
            commentLine_ = false;
        }
    }

    std::ifstream file_;
    bool inComment_ = false;


    MD5 fileHash_;

    FileRecord & record_;

    /** Initial state of the matching FSM.
     */
    static MatchStep initialState_;

    /** Either CommentKind::invalid if tokenization is not inside a comment, or comment kind to end the comment if inside comment.
     */
    CommentKind commentEnd_ = CommentKind::none;

    /** True if there was only whitespace in the last line.
     */
    bool emptyLine_ = true;

    /** True if the line contains only comments.
     */
    bool commentLine_ = true;


};


