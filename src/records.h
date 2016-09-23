#ifdef haha
#pragma once


#include <string>
#include <ostream>
#include <atomic>

#include "config.h"


class ProjectRecord {
public:
    // project id
    unsigned int pid;
    // path to the project on local drive
    std::string path;
    // path to the project on github
    std::string url;


    ProjectRecord(std::string const & path, std::string const & url):
        pid(projectID()),
        path(path),
        url(url) {
    }

    /** Writes sourcererCC's bookkeeping information into given output stream.
     */
    void writeSourcererBookkeeping(std::ostream & f) {
        f << pid << ","
          << escapePath(path) << ","
          << escapePath(url) << std::endl;
    }

    static unsigned projectID(bool increment = true) {
        static std::atomic_uint counter(PROJECT_ID_HINT);
        return increment ? (unsigned)counter++ : (unsigned)counter;
    }

private:

};


class FileRecord {
public:
    // project
    ProjectRecord & project;
    // path of the file *in* the project
    std::string path;

    // file id
    unsigned fid = 0;

    // size of the file
    unsigned bytes = 0;
    // number of bytes that store comments
    unsigned commentBytes = 0;
    // number of bytes that used whitespace
    unsigned whitespaceBytes = 0;
    // number of bytes of all tokens
    unsigned tokenBytes = 0;


    // number of lines in the file
    unsigned loc = 0;
    // number of lines that are empty
    unsigned emptyLoc = 0;
    // number of lines that are only comments
    unsigned commentLoc = 0;

    // total number of tokens in the file
    unsigned totalTokens = 0;
    // number of unique tokens in the file
    unsigned uniqueTokens() {
        return tokens_.size();
    }


    std::string fileHash;
    std::string tokensHash;

    /** This writes the file record in the sourcererCC format.

     */
    void writeSourcererFileStats(std::ostream & f) {
        f << project.pid << ","
          << fid << ","
          << escapePath(STR(project.path << "/" << path)) << ","
          << escapePath(STR(project.url << "/blob/master/" << path)) << ","
          << fileHash << ","
          << bytes << ","
          << loc << "," // size in lines
          << loc - emptyLoc << "," // LOC
          << loc - emptyLoc - commentLoc << std::endl; // SLOC
    }


    /** Dumps all the statistics we are calculating with to the given file.

      Note that this is not sourcererCC's format, but ours.
     */
    void writeFileRecord(std::ostream & f) {
        f << project.pid << ","
          << fid << ","
          << escapePath(project.path) << ","
          << escapePath(path) << ","
          << bytes << ","
          << commentBytes << ","
          << whitespaceBytes << ","
          << tokenBytes << ","
          << (bytes - commentBytes - whitespaceBytes - tokenBytes) << "," // separator bytes
          << loc << ","
          << commentLoc << ","
          << emptyLoc << ","
          << totalTokens << ","
          << uniqueTokens() << ","
          << fileHash << ","
          << tokensHash << std::endl;
    }

    /** Outputs the tokens file for sourcererCC.
     */
    void writeSourcererFileTokens(std::ostream & f) {
        // if the file is empty, do not output it as it causes problems for sourcerer
        if (tokens_.size() == 0)
            return;
        f << project.pid << ","
          << fid << ","
          << totalTokens << ","
          << uniqueTokens() << ","
          << tokensHash << "@#@";
        auto i = tokens_.begin(), e = tokens_.end();
        f << escape(i->first) << "@@::@@" << i->second;
        ++i;
        while (i != e) {
            f << "," << escape(i->first) << "@@::@@" << i->second;
            ++i;
        }
        f << std::endl;
    }



    FileRecord(ProjectRecord & project, std::string const & path):
        project(project),
        path(path),
        fid(fileID()) {
    }

    static unsigned fileID(bool increment = true) {
        static std::atomic_uint counter(FILE_ID_HINT);
        return increment ? (unsigned)counter++ : (unsigned)counter;
    }



private:

    friend class FileTokenizer;

    std::map<std::string, unsigned> tokens_;

};


struct OutputFiles {
    std::ofstream bookkeeping;
    std::ofstream stats;
    std::ofstream tokens;
    std::ofstream ourdata;

    OutputFiles(std::string outputDir, unsigned index):
        bookkeeping(STR(outputDir << "/" << PATH_BOOKKEEPING_PROJS << "/bookkeeping-proj-" << index << ".txt")),
        stats(STR(outputDir << "/" << PATH_STATS_FILE << "/files-stats-" << index << ".txt")),
        tokens(STR(outputDir << "/" << PATH_TOKENS_FILE << "/files-tokens-" << index << ".txt")),
        ourdata(STR(outputDir << "/" << PATH_OUR_DATA_FILE << "/files-" << index << ".txt"))
    {
        if (not bookkeeping.good() or not stats.good() or not tokens.good())
            throw STR("Unable to create sourcererCC output files");
    }
};



#endif
