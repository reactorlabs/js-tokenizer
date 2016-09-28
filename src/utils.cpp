#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>

#include "utils.h"

char toHexDigit(unsigned from) {
    return from >= 10 ? 'a' + from - 10 : '0' + from;
}

unsigned fromHexDigit(char from) {
    return (from >= 'a') ? from - 'a' + 10 : from - '0';
}


void escape(char c, std::string & into) {
    into += '%';
    char hi = ((unsigned char) c) / 16;
    char lo = ((unsigned char) c) % 16;
    into += toHexDigit(hi);
    into += toHexDigit(lo);
}


std::string escapeToken(std::string const & token) {
    std::string result;
    result.reserve(token.size());
    for (char c : token) {
        if ((c >= '0' and c <= '9') or (c >= 'a' and c <= 'z') or ( c >= 'A' and c <= 'Z'))
            result += c;
        else
            escape(c, result);
    }
    return result;

}


std::string escapePath(std::string const & from) {
    std::string result;
    result.reserve(from.size());
    for (char c : from) {
        switch (c) {
            case ',':
            case ' ':
            case '#':
            case '@':
            case '%':
                escape(c, result);
                break;
            default:
                result += c;
        }
    }
    return result;
}

std::string unescapePath(std::string const & from) {
    std::string result;
    result.reserve(from.size());
    size_t i = 0, e = from.size();
    while (i < e) {
        if (from[i] != '%') {
            result += from[i];
            ++i;
        } else {
            result += (char) (fromHexDigit(from[i+1]) * 16 + fromHexDigit(from[i+2]));
            i += 3;
        }
    }
    return result;
}

#include <iostream>

std::string loadEntireFile(std::string const & filename) {
    std::ifstream s(filename, std::ios::in | std::ios::binary);
    if (not s.good())
        throw STR("Unable to open file " << filename);
    s.seekg(0, std::ios::end);
    std::string result;
    result.resize(s.tellg());
    s.seekg(0, std::ios::beg);
    s.read(& result[0], result.size());
    s.close();
    return result;
}

std::vector<std::string> split(std::string const & what, char delimiter) {
    std::vector<std::string> result;
    int start = 0;
    while (start < what.size()) {
        int end = what.find(delimiter, start);
        if (end < 0)
            end = what.size();
        result.push_back(what.substr(start, end-start));
        start = end + 1;
    }
    return result;
}






bool isDirectory(std::string const & path) {
    struct stat s;
    if (lstat(path.c_str(),&s) == 0 ) {
        if( s.st_mode & S_IFDIR )
            return true;
    }
    return false;
}

void createDirectory(std::string const & path) {
    if (system(STR("mkdir -p " << path).c_str()) != EXIT_SUCCESS)
        throw STR("Unable to create directory " << path);
}



