#include <sys/types.h>
#include <sys/stat.h>

#include <memory>

#include <iostream>
#include <iomanip>
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
    if (lstat(path.c_str(),&s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}

bool isFile(std::string const & path) {
    struct stat s;
    if (lstat(path.c_str(),&s) == 0)
        return S_ISREG(s.st_mode);
    return false;
}



void createDirectory(std::string const & path) {
    if (system(STR("mkdir -p " << path).c_str()) != EXIT_SUCCESS)
        throw STR("Unable to create directory " << path);
}

double secondsSince(std::chrono::high_resolution_clock::time_point start) {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0;
}

/** Nice time printer.
 */
std::string time(double sec) {
    unsigned s = static_cast<unsigned>(sec);
    unsigned m = s / 60;
    unsigned h = m / 60;
    s = s % 60;
    m = m % 60;
    return STR(h << ":" << std::setfill('0') << std::setw(2) << m << ":"  << std::setfill('0') << std::setw(2)<< s);
}

std::string exec(std::string const & what, std::string const & path ) {
    char buffer[128];
    std::string result = "";
    std::string cmd = STR("cd \"" << path << "\" && " << what << " 2>&1");
    //std::cout << cmd << std::endl;
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (not pipe)
        throw STR("Unable to execute command " << cmd);
    while (not feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != nullptr)
            result += buffer;
    }
    return result;
}



