#pragma once

#include <sstream>
#include <iostream>
#include "../hashes/md5.h"

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
