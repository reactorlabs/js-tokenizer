#pragma once
#include <string>
#include <vector>
#include <iostream>

#include "utils.h"
#include "config.h"

class ProjectInfo {
public:


private:
    friend class FileInfo;

    unsigned id_;
    std::string path_;
    std::string github_;

};




class FileInfo {
public:



    void writeStatistics(std::ostream & s);


private:
    unsigned id_;

    ProjectInfo * project_;

    std::string relPath_;

    unsigned bytes_ = 0;
    unsigned commentBytes_ = 0;
    unsigned whitespaceBytes_ = 0;
    unsigned tokenBytes_ = 0;
    unsigned separatorBytes_ = 0;

    unsigned loc_ = 0;
    unsigned commentLoc_ = 0;
    unsigned emptyLoc_ = 0;

    unsigned totalTokens_ = 0;
    unsigned uniqueTokens_ = 0;

    unsigned errors_ = 0;

    std::string fileHash_;
    std::string tokensHash_;

};

class FileStatistic {
public:
    static void parseFile(std::string const & filename);

    static FileStatistic * get(size_t id) {
        id -= FILE_ID_STARTS_AT;
        assert(id < files_.size());
        return files_[id];
    }

    static size_t numFiles() {
        return files_.size();
    }

    std::string absPath() const {
        return projectPath_ + "/" + filePath_;
    }

    std::string const & fileHash() const {
        return fileHash_;
    }


private:
    FileStatistic() = default;

    bool load(std::istream & s);

    unsigned id_;
    unsigned pid_;
    std::string projectPath_;
    std::string filePath_;
    unsigned bytes_ = 0;
    unsigned commentBytes_ = 0;
    unsigned whitespaceBytes_ = 0;
    unsigned tokenBytes_ = 0;
    unsigned separatorBytes_ = 0;

    unsigned loc_ = 0;
    unsigned commentLoc_ = 0;
    unsigned emptyLoc_ = 0;

    unsigned totalTokens_ = 0;
    unsigned uniqueTokens_ = 0;

    unsigned errors_ = 0;

    std::string fileHash_;
    std::string tokensHash_;

    static std::vector<FileStatistic *> files_;
};

class CloneInfo {
public:
    static void parseFile(std::string const & filename);

    static CloneInfo * get(size_t index) {
        assert(index < clones_.size());
        return clones_[index];
    }

    static size_t numClones() {
        return clones_.size();
    }

    unsigned fid1() const {
        return fid1_;
    }

    unsigned fid2() const {
        return fid2_;
    }


private:
    CloneInfo() = default;


    bool load(std::istream & s);

    unsigned pid1_;
    unsigned fid1_;
    unsigned pid2_;
    unsigned fid2_;

    static std::vector<CloneInfo *> clones_;
};



