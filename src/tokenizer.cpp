#include "tokenizer.h"


FileTokenizer::MatchStep FileTokenizer::initialState_;

void FileTokenizer::initializeLanguage() {
    MatchStep & start = FileTokenizer::initialState_;
    unsigned w = 0;
    unsigned c = 0;
    unsigned s = 0;
#define GENERATE_WHITESPACE_MATCH(X) start.addMatch(X, CommentKind::none, false, true); ++w;
#define GENERATE_COMMENTS_MATCH(START, END, NAME) start.addMatch(START, CommentKind::NAME ## _start, false, false); \
    start.addMatch(END, CommentKind::NAME ## _end, false, false); ++c;
#define GENERATE_SEPARATORS_MATCH(X) start.addMatch(X, CommentKind::none, true, false); ++s;
    WHITESPACE(GENERATE_WHITESPACE_MATCH)
    COMMENTS(GENERATE_COMMENTS_MATCH)
    SEPARATORS(GENERATE_SEPARATORS_MATCH)

    std::cout << "Initialization for language " << LANGUAGE << " done." << std::endl;
    std::cout << "    " << w << " whitespace matches" << std::endl;
    std::cout << "    " << c << " comment types" << std::endl;
    std::cout << "    " << s << " separators" << std::endl;
}
