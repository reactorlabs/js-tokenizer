#pragma once
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <chrono>
#include <cmath>
#include <iomanip>

#include "hashes/md5.h"

/** Shorthand for converting different types to string as long as they support the std::ostream << operator.
*/
#define STR(WHAT) static_cast<std::stringstream&>(std::stringstream() << WHAT).str()


/** Compressed hash so that it occupies less space.
 */
struct hash {
    uint64_t first;
    uint64_t second;

    hash():
        first(0),
        second(0) {
    }

    /** Constructs the hash from given string as computed by the hashlibrary.
     */
    hash(MD5 & from) {
        from.getHash(reinterpret_cast<unsigned char *>(this));
    }

    hash(std::string const & from) {
        MD5 md5;
        md5.add(from.c_str(), from.size());
        md5.getHash(reinterpret_cast<unsigned char *>(this));
    }

    hash(hash const & from):
        first(from.first),
        second(from.second) {
    }

    hash & operator = (hash const & other) {
        first = other.first;
        second = other.second;
        return *this;
    }

    bool operator == (hash const & other) const {
        return first == other.first and second == other.second;
    }

    bool operator != (hash const & other) const {
        return first != other.first or second != other.second;
    }

    friend std::ostream & operator << (std::ostream & s, hash const & h) {
        static const char dec2hex[16+1] = "0123456789abcdef";
        unsigned char const * x = reinterpret_cast<unsigned char const *>(&h);
        for (int i = 0; i < 32; i++) {
            s << dec2hex[(x[i] >> 4) & 15];
            s << dec2hex[(x[i]) & 15];
        }
        return s;
    }
};

namespace std {

    template<>
    struct hash<::hash> {

        std::size_t operator()(::hash const & h) const {
            return std::hash<uint64_t>{}(h.first) + std::hash<uint64_t>{}(h.second);
        }

    };


}

inline unsigned decDigit(char from) {
    return from - '0';
}

inline unsigned timestampFrom(std::string col) {
    // TODO return something meaningful
}

inline std::string escape(hash const & what) {
    return STR("\"" << std::hex << std::setfill('0') << std::setw(16) << what.first << what.second << "\"");
}

inline std::string escape(std::string const & from) {
    std::string result;
    result.reserve(from.size());
    result += "\"";
    for (char c : from) {
        switch (c) {
            case 0:
                result += "\\0";
                break;
            case '\'':
            case '\"':
            case '\\':
            case '_':
            case '%':
                result += "\\" + c;
                break;
            case '\b':
                result += "\\b";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            case 26:
                result += "\\Z";
                break;
            default:
                result += c;
        }
    }
    result += "\"";
    return result;
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
    if (total == 0)
        return "";
    double pct = (x * 100.0) / total;
    if (pct > 10)
        return STR(round(pct));
    else
        return STR(round(pct * 10) / 10);
}

/** Execute command and grab its output.
 */
std::string exec(std::string const & what, std::string const & path);
