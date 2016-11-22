#include <fstream>

#include "hashes/md5.h"

#include "data.h"















#ifdef HAHA




// ProjectInfo -----------------------------------------------------------------

std::vector<GitProject *> GitProject::projects_;

void GitProject::parseFile(std::string const & filename) {
    std::ifstream f(filename);
    if (not f.good())
        throw STR("Unable to open prtoject info file " << filename);
    while (not f.eof()) {
        std::string tmp;
        std::getline(f, tmp, '\n');
        if (tmp == "")
            break; // eof
        GitProject * pi = new GitProject();
        pi->loadFrom(tmp);
        size_t id = pi->id_ - PROJECT_ID_STARTS_AT;
        if (id >= projects_.size())
            projects_.resize(id + 1);
        projects_[id] = pi;
    }
}

void GitProject::loadFrom(std::string const & tmp) {
    std::vector<std::string> items(split(tmp, ','));
    try {
        if (items.size() != 3)
            throw "";
        id_ = std::stoi(items[0]);

        path_ = unescapePath(items[1]);
        url_ = unescapePath(items[2]);

    } catch (...) {
        throw "Invalid format of statistics file";
    }
}





// FileStatistic ---------------------------------------------------------------

std::vector<FileStats *> FileStats::files_;

void FileStats::parseFile(std::string const & filename) {
    std::ifstream f(filename);
    if (not f.good())
        throw STR("Unable to open file statistics " << filename);
    while (not f.eof()) {
        std::string tmp;
        std::getline(f, tmp, '\n');
        if (tmp == "")
            break; // eof
        FileStats * fs = new FileStats();
        fs->loadFrom(tmp);
        size_t id = fs->id_ - FILE_ID_STARTS_AT;
        if (id >= files_.size())
            files_.resize(id + 1);
        files_[id] = fs;
    }
}

void FileStats::loadFrom(std::string const & tmp) {
    std::vector<std::string> items(split(tmp, ','));
    try {
        if (items.size() != 17)
            throw STR("Invalid line format");
        id_ = std::stoi(items[0]);
        project_ = GitProject::Get(std::stoi(items[1]));
        if (unescapePath(items[2]) != project_->path())
            throw STR("File " << id_ << " contains invalid path for its project");

        relPath_ = unescapePath(items[3]);

        bytes_ = std::stoi(items[4]);
        commentBytes_ = std::stoi(items[5]);
        whitespaceBytes_ = std::stoi(items[6]);
        tokenBytes_ = std::stoi(items[7]);
        separatorBytes_ = std::stoi(items[8]);

        loc_ = std::stoi(items[9]);
        commentLoc_ = std::stoi(items[10]);
        emptyLoc_ = std::stoi(items[11]);

        totalTokens = std::stoi(items[12]);
        uniqueTokens_ = std::stoi(items[13]);

        errors = std::stoi(items[14]);

        fileHash_ = items[15];
        tokensHash_ = items[16];
    } catch (std::string const & e) {
        throw e;
    } catch (...) {
        throw "Invalid format of statistics file";
    }
}

void FileStats::writeFullStats(std::ostream & s) {
    s << id_ << ","
      << project_->id_ << ","
      << escapePath(project_->path()) << ","
      << escapePath(relPath_) << ","
      << escapePath(githubUrl()) << ","
      << createdDate << ","
      << bytes_ << ","
      << commentBytes_ << ","
      << whitespaceBytes_ << ","
      << tokenBytes_ << ","
      << separatorBytes_ << ","
      << loc_ << ","
      << commentLoc_ << ","
      << emptyLoc_ << ","
      << totalTokens << ","
      << uniqueTokens_ << ","
      << errors << ","
      << fileHash_ << ","
      << tokensHash_ << std::endl;
}

void FileStats::writeSourcererStats(std::ostream & s) {
    s << project_->id_ << ","
      << id_ << ","
      << escapePath(absPath()) << ","
      << escapePath(githubUrl()) << ","
      << fileHash_ << ","
      << bytes_ << ","
      << loc_ << ","
      << (loc_ - emptyLoc_) << ","
      << (loc_ - emptyLoc_ - commentLoc_) << std::endl;
}

// TokenMap --------------------------------------------------------------------

std::string TokenMap::calculateHash() {
    MD5 md5;
    for (auto i : freqs_) {
        md5.add(i.first.c_str(), i.first.size());
        md5.add(& i.second, sizeof(i.second));
    }
    return md5.getHash();
}

void TokenMap::writeSourcererFormat(std::ostream & s) {
    if (not freqs_.empty()) {
        auto i = freqs_.begin(), e = freqs_.end();
        s << i->first << "@@::@@" << i->second;
        ++i;
        while (i != e) {
            s << "," << i->first << "@@::@@" << i->second;
            ++i;
        }
    }
}


// TokenizedFile ---------------------------------------------------------------

void TokenizedFile::updateFileStats(std::string const & contents) {
    stats.bytes_ = contents.size();
    MD5 md5;
    md5.add(contents.c_str(), contents.size());
    stats.fileHash_ = md5.getHash();
}

void TokenizedFile::writeTokens(std::ostream & s) {
    s << stats.project_->id_ << ","
      << stats.id_ << ","
      << stats.totalTokens << ","
      << stats.uniqueTokens_ << ","
      << stats.tokensHash_ << "@#@";
    tokens.writeSourcererFormat(s);
    s << std::endl;
}



// CloneInfo -------------------------------------------------------------------

std::vector<CloneInfo *> CloneInfo::clones_;

void CloneInfo::parseFile(std::string const & filename) {
    std::ifstream f(filename);
    if (not f.good())
        throw STR("Unable to open clone info " << filename);
    while (not f.eof()) {
        std::string tmp;
        std::getline(f, tmp, '\n');
        if (tmp == "")
            break; // eof
        CloneInfo * ci = new CloneInfo();
        ci->loadFrom(tmp);
        clones_.push_back(ci);
    }
}

void CloneInfo::loadFrom(std::string const & tmp) {
    std::vector<std::string> items(split(tmp, ','));
    try {
        pid1_ = std::stoi(items[0]);
        fid1_ = std::stoi(items[1]);
        pid2_ = std::stoi(items[2]);
        fid2_ = std::stoi(items[3]);
    } catch (...) {
        throw "Invalid format of clone info file";
    }
}

// CloneGroup ------------------------------------------------------------------

std::vector<CloneGroup *> CloneGroup::groups_;
std::unordered_map<unsigned, CloneGroup *> CloneGroup::idToGroup_;

void CloneGroup::find() {
    for (size_t i = 0, e = CloneInfo::numClones(); i != e; ++i) {
        CloneInfo * ci = CloneInfo::get(i);
        auto i1 = idToGroup_.find(ci->fid1());
        auto i2 = idToGroup_.find(ci->fid2());
        if (i1 == i2) {
            if (i1 == idToGroup_.end()) {
                CloneGroup * cg = new CloneGroup();
                groups_.push_back(cg);
                cg->ids_.insert(ci->fid1());
                cg->ids_.insert(ci->fid2());
                idToGroup_[ci->fid1()] = cg;
                idToGroup_[ci->fid2()] = cg;
            }
            // otherwise we have already seen the match and do nothing
        } else if (i1 == idToGroup_.end()) {
            // add first to second's group
            i2->second->ids_.insert(ci->fid1());
            idToGroup_[ci->fid1()] = i2->second;
        } else if (i2 == idToGroup_.end()) {
            // add second to first's group
            i1->second->ids_.insert(ci->fid2());
            idToGroup_[ci->fid2()] = i1->second;
        } else {
            // we have two groups that should be joined
            for (unsigned id : i2->second->ids_) {
                i1->second->ids_.insert(id);
                idToGroup_[id] = i1->second;
            }
            // delete second group
            auto old = groups_.begin();
            while (*old != i2->second)
                ++old;
            delete i2->second;
            groups_.erase(old);
        }
    }
}

void CloneGroup::writeTo(std::ostream & s) {
    auto i = ids_.begin();
    s << *i;
    ++i;
    while (i != ids_.end()) {
        s << "," << *i;
        ++i;
    }
    s << std::endl;
}

void CloneGroup::writeStats(std::ostream & s) {
    s << *ids_.begin() << ","
      << ids_.size() << std::endl;

}


#endif
