#include <fstream>

#include "data.h"







void FileInfo::writeStatistics(std::ostream & s) {
    s << id_ << ","
      << project_->id_ << ","
      << escapePath(project_->path_) << ","
      << escapePath(relPath_) << ","
      << bytes_ << ","
      << commentBytes_ << ","
      << whitespaceBytes_ << ","
      << tokenBytes_ << ","
      << separatorBytes_ << ","
      << loc_ << ","
      << commentLoc_ << ","
      << emptyLoc_ << ","
      << totalTokens_ << ","
      << uniqueTokens_ << ","
      << errors_ << ","
      << fileHash_ << ","
      << tokensHash_ << std::endl;
}



// FileStatistic ---------------------------------------------------------------

std::vector<FileStatistic *> FileStatistic::files_;

void FileStatistic::parseFile(std::string const & filename) {
    std::ifstream f(filename);
    if (not f.good())
        throw STR("Unable to open file statistics " << filename);
    while (not f.eof()) {
        FileStatistic * fs = new FileStatistic();
        if (not fs->load(f)) {
            assert(f.eof());
            delete fs;
            break;
        }
        size_t id = fs->id_ - FILE_ID_STARTS_AT;
        if (id >= files_.size())
            files_.resize(id + 1);
        files_[fs->id_ - FILE_ID_STARTS_AT] = fs;
    }
}

bool FileStatistic::load(std::istream & s) {
    assert(not s.eof());
    std::string tmp;
    std::getline(s, tmp, '\n');
    if (tmp == "")
        return false;
    std::vector<std::string> items(split(tmp, ','));
    try {
        if (items.size() != 17)
            throw "";
        pid_ = std::stoi(items[0]);
        id_ = std::stoi(items[1]);

        projectPath_ = unescapePath(items[2]);
        filePath_ = unescapePath(items[3]);

        bytes_ = std::stoi(items[4]);
        commentBytes_ = std::stoi(items[5]);
        whitespaceBytes_ = std::stoi(items[6]);
        tokenBytes_ = std::stoi(items[7]);
        separatorBytes_ = std::stoi(items[8]);

        loc_ = std::stoi(items[9]);
        commentLoc_ = std::stoi(items[10]);
        emptyLoc_ = std::stoi(items[11]);

        totalTokens_ = std::stoi(items[12]);
        uniqueTokens_ = std::stoi(items[13]);

        errors_ = std::stoi(items[14]);

        fileHash_ = items[15];
        tokensHash_ = items[16];

        return true;
    } catch (...) {
        throw "Invalid format of statistics file";
    }
}



// CloneInfo -------------------------------------------------------------------

std::vector<CloneInfo *> CloneInfo::clones_;

void CloneInfo::parseFile(std::string const & filename) {
    std::ifstream f(filename);
    if (not f.good())
        throw STR("Unable to open clone info " << filename);
    while (not f.eof()) {
        CloneInfo * ci = new CloneInfo();
        if (not ci->load(f)) {
            assert(f.eof());
            delete ci;
            break;
        }
        clones_.push_back(ci);
    }
}


bool CloneInfo::load(std::istream & s) {
    assert(not s.eof());
    std::string tmp;
    std::getline(s, tmp, '\n');
    if (tmp == "")
        return false;
    std::vector<std::string> items(split(tmp, ','));
    try {
        pid1_ = std::stoi(items[0]);
        fid1_ = std::stoi(items[1]);
        pid2_ = std::stoi(items[2]);
        fid2_ = std::stoi(items[3]);
        return true;
    } catch (...) {
        throw "Invalid format of clone info file";
    }
}

