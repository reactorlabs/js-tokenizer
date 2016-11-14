#include "generic.h"
#include "../worker.h"

// "; . [ ] ( ) ~ ! - + & * / % < > & ^ | ? { } = # , " \ : $ '"


bool GenericTokenizer::eof() {
    return pos_ >= data_.size();
}

char GenericTokenizer::top() {
    if (pos_ >= data_.size())
        return 0;
    if (data_[pos_] == '\n') {
        if (peek(1) == '\r') {
            data_[pos_ + 1] = '\n';
            ++pos_;
        }
    } else if (data_[pos_] == '\r') {
        if (peek(1) == '\n')
            ++pos_;
    }
    return data_[pos_];
}

void GenericTokenizer::pop(unsigned by) {
    pos_ += by;
    if (pos_ > data_.size())
        pos_ = data_.size();
}

char GenericTokenizer::peek(int offset) {
    if (pos_ + offset < 0 or pos_ + offset >= data_.size())
        return 0;
    return data_[pos_ + offset];
}


void GenericTokenizer::addToken(unsigned start, unsigned length) {
    if (length > 0) {
        hasToken_ = true;
        f_.addToken(data_.substr(start, length));
    }
}

void GenericTokenizer::newline() {
    ++f_.loc;
    if (not hasToken_) {
        if (hasComment_)
            ++f_.commentLoc;
        else
            ++f_.emptyLoc;
    }
    hasComment_ = false;
    hasToken_ = false;
}

void GenericTokenizer::tokenize() {
    pos_ = 0;
    hasComment_ = false;
    hasToken_ = false;
    unsigned start = 0;
    unsigned tokenLength_ = 0;
    while (not eof()) {
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
                    if (top() == '*' and peek(1) == '/')
                        break;
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
            case '\t':
            case ' ':
            case '\r':
            case '\n':
                addToken(start, tokenLength_);
                if (top() == '\n')
                    newline();
                pop();
                start = pos_;
                tokenLength_ = 0;
                break;
            default:
                pop();
                ++tokenLength_;
                break;
        }
    }
    addToken(start, tokenLength_);
}

void GenericTokenizer::loadEntireFile() {
    std::ifstream s(f_.path(), std::ios::in | std::ios::binary);
    if (not s.good())
        throw STR("Unable to open file " << f_.path());
    s.seekg(0, std::ios::end);
    data_.resize(s.tellg());
    s.seekg(0, std::ios::beg);
    s.read(& data_[0], data_.size());
    s.close();
    if (data_.size() >= 4 and data_[0] == 'P' and data_[1] =='K' and data_[2] == '\003' and data_[3] == '\004')
        throw STR("File " << f_.path() << " seems to be archive");
}











#ifdef HAHA
#include "../old/worker.h"
#include "generic.h"


class Tokenizer::MatchStep {
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
        Kind kind = Kind::none;
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
        void addMatch(std::string const & what, Kind kind, bool isSeparator, bool isWhitespace) {
            MatchStep * state = this;
            for (size_t i = 0, e = what.size(); i != e; ++i) {
                auto it = state->next_.find(what[i]);
                if (it == state->next_.end())
                    it = state->next_.insert(std::pair<char, MatchStep *>(what[i], new MatchStep())).first;
                state = it->second;
            }
            if (kind != Kind::none)
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

Tokenizer::MatchStep * Tokenizer::initialState_;

void Tokenizer::initializeLanguage() {
    initialState_ = new MatchStep();
    MatchStep & start = *initialState_;
    unsigned w = 0;
    unsigned c = 0;
    unsigned s = 0;
    unsigned l = 0;
#define GENERATE_WHITESPACE_MATCH(X) start.addMatch(X, Kind::none, false, true); ++w;
#define GENERATE_COMMENTS_MATCH(START, END, NAME) start.addMatch(START, Kind::NAME ## _start, false, false); \
    start.addMatch(END, Kind::NAME ## _end, false, false); ++c;
#define GENERATE_LITERALS_MATCH(START, ESCAPE, NAME) start.addMatch(START, Kind::NAME ## _start, false, false); \
    start.addMatch(ESCAPE, Kind::NAME ## _escape, false, false); ++l;

#define GENERATE_SEPARATORS_MATCH(X) start.addMatch(X, Kind::none, true, false); ++s;
    WHITESPACE(GENERATE_WHITESPACE_MATCH)
    COMMENTS(GENERATE_COMMENTS_MATCH)
    LITERALS(GENERATE_LITERALS_MATCH)
    SEPARATORS(GENERATE_SEPARATORS_MATCH)

    Worker::print(STR("Initialization for language " << LANGUAGE << " done."));
    Worker::print(STR("    " << w << " whitespace matches"));
    Worker::print(STR("    " << c << " comment types"));
    Worker::print(STR("    " << s << " separators"));
    Worker::print(STR("    " << l << " literals"));
}

char Tokenizer::get() {
    char result = file_.get();
    if (result == '\r' and file_.peek() == '\n') {
        result = file_.get();
    } else if (result == '\n' and file_.peek() == '\r') {
        file_.get();
    }
    // increase total bytes (ignore different line endings
    ++f_.bytes_;
    // add the byte to the file hash
    // TODO this ignores extra bytes in new lines, is this acceptable?
    fileHash_.add(&result, 1);
    return result;
}

void Tokenizer::tokenize() {
    commentEnd_ = Kind::none;
    emptyLine_ = true;
    commentLine_ = true;
    std::string temp;
    MatchStep * state = initialState_;
    MatchStep * lastMatch = nullptr;
    unsigned lastMatchPos = 0;
    commentEnd_ = Kind::none;
    while (true) {
        char c = get();
        if (file_.eof() and c == -1)
            break;
        state = state->next(c);
        temp += c;
        // if the character cannot be matched
        if (state == nullptr) {
            // try matching the character from the beginning
            state = initialState_->next(c);
            // if it is not from the beginning, start from beginning for next character
            if (state == nullptr)
                state = initialState_;
            // if we have matched something, it is the longest one, process the match
            if (lastMatch != nullptr) {
                processMatch(lastMatch, lastMatchPos, temp);
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
        processMatch(lastMatch, lastMatchPos, temp);
    if (not temp.empty())
        addToken(temp);
    // line numbers start from 1 actually
    if (f_.bytes_ > 0) {
        ++f_.loc_;
        --f_.bytes_;
    }
}

void Tokenizer::processMatch(MatchStep * match, unsigned matchPos, std::string & temp) {
    // first if we are not in special mode, process previous token, if any
    unsigned x = matchPos - match->length;
    if (commentEnd_ == Kind::none) {
        // first check if we have token
        if (x > 0) {
            addToken(temp.substr(0, x));
        }
    }
    // deal with new line
    if (temp[x] == '\n') {
        if (commentLine_ and inComment())
            ++f_.commentLoc_;
        else if (emptyLine_)
            ++f_.emptyLoc_;
        ++f_.loc_;
        emptyLine_ = true;
        commentLine_ = not inLiteral();
    }

    // if we are not in special mode, process tokens accordingly
    if (commentEnd_ == Kind::none) {
        // see if we are entering a comment or literal
        commentEnd_ = kindEndFor(match->kind);
        if (commentEnd_ != Kind::none) {
            // do not match the opening separator now
            matchPos -= match->length;
            emptyLine_ = false;
            if (not inComment())
                commentLine_ = false;
        } else if (match->isSeparator) {
            commentLine_ = false;
            emptyLine_ = false;
            f_.separatorBytes_ += match->length;
        } else if (match->isWhitespace) {
            f_.whitespaceBytes_ += match->length;
        } else {
            // default is separator for nonsensical characters
            commentLine_ = false;
            emptyLine_ = false;
            f_.separatorBytes_ += match->length;
        }
    // in special mode, do nothing unless we have reached the end token
    } else if (commentEnd_ == match->kind) {
        if (inComment()) {
            f_.commentBytes_ += matchPos;
        } else {
            // add the literal as one token
            addToken(temp.substr(0, matchPos));
        }
        commentEnd_ = Kind::none;
    } else {
        matchPos = 0;
    }
    // remove any matched token or separator from the temp string
    if (matchPos != 0)
        temp = temp.substr(matchPos);

    return;




    // first check if we have token
    x = matchPos - match->length;
    if (x > 0) {
        addToken(temp.substr(0, x));
    }
    // deal with new lines
    if (temp[x] == '\n') {
        if (commentLine_ and inComment())
            ++f_.commentLoc_;
        else if (emptyLine_)
            ++f_.emptyLoc_;
        ++f_.loc_;
        emptyLine_ = true;
        commentLine_ = true;
    }
    // add the matched stuff and deal with it appropriately
    if (inComment()) {
        f_.commentBytes_ += match->length;
        if (match->kind == commentEnd_) {
            commentEnd_ = Kind::none;
        }
    // if we are not in a comment, we may start a new one
    } else {
        if (match->kind != Kind::none)
            commentEnd_ = kindEndFor(match->kind);
        // if we have comment end, we are inside comment
        if (inComment()) {
            f_.commentBytes_ += match->length;
        } else if (match->isSeparator) {
            commentLine_ = false;
            emptyLine_ = false;
        } else if (match->isWhitespace) {
            f_.whitespaceBytes_ += match->length;
        }
    }
    // now remove the potential token and the matched token afterwards
    temp = temp.substr(matchPos);
}

void Tokenizer::addToken(std::string const & token) {
    emptyLine_ = false;
    if (inComment()) {
        f_.commentBytes_ += token.size();
    } else {
        f_.tokenBytes_ += token.size();
        ++f_.totalTokens_;
        f_.tokens_[token] += 1;
        commentLine_ = false;
    }
}

#endif
