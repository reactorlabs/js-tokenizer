#pragma once

#include <dirent.h>

#include "tokenizer.h"

/** Crawls the project, obtains the required metadata and tokenizes all its javascript files.
 */
class ProjectCrawler {
public:
    ProjectCrawler(std::string const & path, std::string const & url, OutputFiles & output):
        record_(path, url),
        output_(output) {
    }

    void crawl() {
        DIR * dir = opendir(record_.path.c_str());
        if ( dir != nullptr) {
            crawlDirectory(dir, record_.path, "");
            record_.writeSourcererBookkeeping(output_.bookkeeping);
        }
    }

private:

    void crawlDirectory(DIR * dir, std::string path, std::string relPath) {
        struct dirent * ent;
        while ((ent = readdir(dir)) != nullptr) {
            // skip . and ..
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            std::string p = path + "/" + ent->d_name;
            std::string rp = relPath.empty() ? ent->d_name : relPath + "/" + ent->d_name;
            // if it is directory, crawl it recursively
            DIR * d = opendir(p.c_str());
            if (d != nullptr) {
                crawlDirectory(d, p, rp);
            // otherwise it is a file, process and deal with accordingly
            } else if (isLanguageFile(ent->d_name)) {
                tokenizeFile(rp);
            }
        }
        closedir(dir);
    }

    void tokenizeFile(std::string const & path) {
        FileRecord r(record_, path);
        FileTokenizer t(r);
        if (t.tokenize()) {
            if (t.record().uniqueTokens() == 0)
                ++empty_files;
            m.lock();
            unsigned fileMatch = exactFileMatches[t.record().fileHash]++;
            unsigned tokensMatch = exactTokenMatches[t.record().tokensHash]++;
            m.unlock();
            if (fileMatch > 1)
                ++exact_files;
            if (tokensMatch > 1)
                ++exact_tokens;
            // output sourcererCC data
            sourcererCCOutput(r, fileMatch == 0, tokensMatch == 0);
            // output our data
            r.writeFileRecord(output_.ourdata);
        } else {
            ++error_files;
        }
    }

    void sourcererCCOutput(FileRecord & t, bool uniqueFile, bool uniqueTokens) {
#if SOURCERERCC_IGNORE_EMPTY_FILES == 1
        if (t.uniqueTokens() == 0)
            return;
#endif
#if SOURCERERCC_IGNORE_FILE_HASH_EQUIVALENTS == 1
        if (not uniqueFile)
            return;
#endif
#if SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS == 1
        if (not uniqueTokens)
            return;
#endif
        t.writeSourcererFileTokens(output_.tokens);
        t.writeSourcererFileStats(output_.stats);
    }


    ProjectRecord record_;
    OutputFiles & output_;
};
