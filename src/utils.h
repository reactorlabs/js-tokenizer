#pragma once
#include <vector>
#include <string>
#include <cassert>
#include <sstream>

/** Shorthand for converting different types to string as long as they support the std::ostream << operator.
*/
#define STR(WHAT) static_cast<std::stringstream&>(std::stringstream() << WHAT).str()


char toHexDigit(unsigned from);

unsigned fromHexDigit(char from);

void escape(char c, std::string & into);

std::string escapePath(std::string const & from);

std::string unescapePath(std::string const & from);


std::string loadEntireFile(std::string const & filename);




std::vector<std::string> split(std::string const & what, char delimiter);

bool isDirectory(std::string const & path);

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
    return endsWith(filename, ".js");

}
