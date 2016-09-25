#pragma once

#include <sys/types.h>
#include <sys/stat.h>

#include <ostream>
#include <sstream>
#include <cstring>

#include <iostream>
#include <iomanip>

#include "config.h"


/** Shorthand for converting different types to string as long as they support the std::ostream << operator.
*/
#define STR(WHAT) static_cast<std::stringstream&>(std::stringstream() << WHAT).str()


/** Outputs the string escaping all non-printale characters.
 */
class escape {
public:
    escape(std::string const & s):
        s(s) {
    }

private:

    void escapeChar(char c, std::ostream &s) const {
        s << "\\" << std::setw(2) << std::hex << (unsigned) c << std::dec;
    }

    friend std::ostream & operator << (std::ostream & s, escape const & str) {
        // escape AND and OR and NOT as they cause problems to sourcererCC
        if (str.s == "AND")
            s << "\\41ND";
        else if (str.s == "OR")
            s << "\\4fR";
        else if (str.s == "NOT")
            s << "\\4eOT";
        else for (size_t i = 0, e = str.s.size(); i != e; ++i) {
            char c = str.s[i];
            // be conservative, escape anything different than number, or letter
            if ((c >= '0' and c <= '9') or (c >= 'a' and c <='z') or (c >= 'A' and c <= 'Z'))
                s << c;
            else
                str.escapeChar(c, s);
/*


            unsigned char c = str.s[i];
            if ((c < ' ') or (c > '~') or c == '#' or c == '@' or c == ',' or c == ':' or c == '\\') {
                s << '\\';
                char x = c / 16;
                s << (char)((x > 9) ? ('a' + x - 10) : ('0' + x));
                x = c % 16;
                s << (char)((x > 9) ? ('a' + x - 10) : ('0' + x));
            } else {
                s << c;
            } */
        }
        return s;
    }

    std::string const & s;
};

/** Outputs the string escaping commas in paths.
 */
class escapePath {
public:
    escapePath(std::string const & s):
        s(s) {
    }

private:

    friend std::ostream & operator << (std::ostream & s, escapePath const & str) {
        for (size_t i = 0, e = str.s.size(); i != e; ++i) {
            unsigned char c = str.s[i];
            if (c == ',' or c == ' ' or c == '#' or c == '@' or c == '%') {
                s << '%';
                unsigned char x = c / 16;
                s << (x > 9) ? 'a' + (x - 10) : '0' + x;
                x = c % 16;
                s << (x > 9) ? 'a' + (x - 10) : '0' + x;
            } else {
                s << c;
            }
        }
        return s;
    }

    std::string const & s;
};

/** Simple helper that checks whether string ends with given characters.
 */
inline bool endsWith(std::string const & str, std::string const & suffix) {
   if (str.length() >= suffix.length()) {
       return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
   } else {
       return false;
   }
}

/** Returns true, if given file contains sources of the tokenizer language.
 */
inline bool isLanguageFile(std::string const & name) {
#define GENERATE_SUFFIX_FILTER_CHECK(X) if (endsWith(name, X)) return false;
    FILTER_SUFFIX(GENERATE_SUFFIX_FILTER_CHECK)
#define GENERATE_SUFFIX_CHECK(X) if (endsWith(name, X)) return true;
    SUFFIX(GENERATE_SUFFIX_CHECK)
    return false;
}

inline void createDirectory(std::string const & path) {
    if (system(STR("mkdir -p " << path).c_str()) != EXIT_SUCCESS)
        throw STR("Unable to create directory " << path);
}

inline bool isDirectory(std::string const & path) {
    struct stat s;
    if (lstat(path.c_str(),&s) == 0 ) {
        if( s.st_mode & S_IFDIR )
            return true;
    }
    return false;
}


