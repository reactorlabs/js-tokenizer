#pragma once

#include <dirent.h>

#include "tokenizer.h"

/** Crawls the project, obtains the required metadata and tokenizes all its javascript files.
 */
class ProjectCrawler {
public:
    ProjectCrawler(std::string const & path, OutputFiles & output):
        record_(path),
        output_(output) {
        record_.url = githubUrl();
    }

    void crawl() {
        DIR * dir = opendir(record_.path.c_str());
        if ( dir != nullptr) {
            crawlDirectory(dir, record_.path, "");
            record_.writeSourcererBookkeeping(output_.bookkeeping);
        }
    }

private:
    /** Extracts the project github url.

      Goes into the `latest/.git/config`, locates the `[remote "origin"]` section and fetches the `url` property.

      This is very crude, but might just work nicely. Another
     */
    std::string githubUrl() {
        std::ifstream x(record_.path + "/.git/config");
        if (not x.good())
            return "NA";
        std::string l;
        while (not x.eof()) {
            x >> l;
            if (l == "[remote") {
                x >> l;
                if (l == "\"origin\"]") {
                    x >> l;
                    if (l == "url") {
                        x >> l;
                        if (l == "=") {
                            x >> l;
                            return convertGitUrlToHTML(l);
                        }
                    }

                }
            }
        }
        return "invalid_config_file";
    }

    std::string convertGitUrlToHTML(std::string url) {
        if (url.substr(url.size() - 4) != ".git")
            return "not_git_repo";
        if (url.substr(0, 15) == "git@github.com:") {
            url = url.substr(15, url.size() - 19);
        } else if (url.substr(0, 17) == "git://github.com/") {
            url = url.substr(17, url.size() - 21);
        } else if (url.substr(0, 19) == "https://github.com/") {
            url = url.substr(19, url.size() - 23);
        } else {
            return "not_git_repo";
        }
        return "https://github.com/" + url;
    }

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
