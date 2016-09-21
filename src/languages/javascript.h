#define LANGUAGE "Javascript"

#define SUFFIX(ENTRY) \
    ENTRY(".js")

#define FILTER_SUFFIX(ENTRY) \
    ENTRY("min.js")

#define WHITESPACE(ENTRY) \
    ENTRY(" ") \
    ENTRY("\t") \
    ENTRY("\n")

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
    ENTRY("\"") \
    ENTRY("\\") \
    ENTRY(":") \
    ENTRY("$") \
    ENTRY("'")
