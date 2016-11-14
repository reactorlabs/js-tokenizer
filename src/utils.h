#pragma once
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <chrono>

/** Shorthand for converting different types to string as long as they support the std::ostream << operator.
*/
#define STR(WHAT) static_cast<std::stringstream&>(std::stringstream() << WHAT).str()


inline unsigned decDigit(char from) {
    return from - '0';
}

inline unsigned timestampFrom(std::string col) {
    // TODO return something meaningful
}



char toHexDigit(unsigned from);

unsigned fromHexDigit(char from);

void escape(char c, std::string & into);

std::string escapeToken(std::string const & token);

std::string escapePath(std::string const & from);

std::string unescapePath(std::string const & from);


std::string loadEntireFile(std::string const & filename);




std::vector<std::string> split(std::string const & what, char delimiter);

bool isDirectory(std::string const & path);
bool isFile(std::string const & path);

void createDirectory(std::string const & path);

/** Simple helper that checks whether string ends with given characters.
 */
inline bool endsWith(std::string const & str, std::string const & suffix) {
   if (str.length() >= suffix.length()) {
       return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
   } else {
       return false;
   }
}

inline bool isLanguageFile(std::string const & filename) {
    // TODO add min.js as well, and basically, make configurable...
    //if (endsWith(filename, ".min.js")) // ignore minjs
    //    return false;
    return endsWith(filename, ".js");

}


double secondsSince(std::chrono::high_resolution_clock::time_point start);

/** Nice time printer.
 */
std::string time(double sec);

inline std::string pct(unsigned x, unsigned total) {
    return STR(" (" << (x * 100.0 / total) <<"%)");
}

/** Execute command and grab its output.
 */
std::string exec(std::string const & what, std::string const & path);
