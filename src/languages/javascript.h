#define LANGUAGE "Javascript"

#define SUFFIX(ENTRY) \
    ENTRY(".js")

#define FILTER_SUFFIX(ENTRY) \
    ENTRY("min.js")

#define WHITESPACE(ENTRY) \
    ENTRY(" ") \
    ENTRY("\t") \
    ENTRY("\n")

// start, end, escape, empty for no escape
#define LITERALS(ENTRY) \
    ENTRY("\"", "\"", "\\", singleQuote) \
    ENTRY("'", "'", "\\", doubleQuote)


#define COMMENTS(ENTRY) \
    ENTRY("//","\n", single) \
    ENTRY("/*", "*/", multi)

#define SEPARATORS(ENTRY) \
    ENTRY(";") \
    ENTRY("::") \
    ENTRY(".") \
    ENTRY(",") \
    ENTRY("->") \
    ENTRY("[") \
    ENTRY("]") \
    ENTRY("(") \
    ENTRY(")") \
    ENTRY("++") \
    ENTRY("--") \
    ENTRY("~") \
    ENTRY("!") \
    ENTRY("-") \
    ENTRY("+") \
    ENTRY("&") \
    ENTRY("*") \
    ENTRY(".*") \
    ENTRY("->*") \
    ENTRY("/") \
    ENTRY("%") \
    ENTRY("<<") \
    ENTRY(">>") \
    ENTRY("<") \
    ENTRY(">") \
    ENTRY("<=") \
    ENTRY(">=") \
    ENTRY("!=") \
    ENTRY("^") \
    ENTRY("|") \
    ENTRY("&&") \
    ENTRY("||") \
    ENTRY("?") \
    ENTRY("==") \
    ENTRY("{") \
    ENTRY("}") \
    ENTRY("=") \
    ENTRY("#") \
    ENTRY(":") \
    ENTRY("$")
