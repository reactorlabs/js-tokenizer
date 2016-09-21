#pragma once

#include <ostream>
#include <sstream>
#include <cstring>

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

    friend std::ostream & operator << (std::ostream & s, escape const & str) {
        for (size_t i = 0, e = str.s.size(); i != e; ++i) {
            unsigned char c = str.s[i];
            if ((c < 32) or (c > 126)) {
                s << '\\';
                char x = c / 16;
                s << (x > 9) ? 'a' + (x - 10) : '0' + x;
                x = c % 16;
                s << (x > 9) ? 'a' + (x - 10) : '0' + x;
            } else if (c == '\\') {
                s << "\\\\";
            } else if (c == '@') {
                s << "\\@";
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

class FileRecord;
class ProjectRecord;
