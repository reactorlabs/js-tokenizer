#pragma once

#include <dirent.h>
#include <cstring>

#include <queue>
#include <mutex>
#include <condition_variable>

#include "utils.h"
#include "data.h"

/** Crawls the directory structures available and tokenizes files in projects.

  All crawlers share the queue of directories to check. When they reach a directory, they do the following:

  - check whether the directory is a valid github project
  - if there is no project, they start new crawler for each of its subdirectories


 */
class Crawler {


private:
    struct DirInfo {
        DIR * dir;
        std::string path;
        DirInfo(DIR * dir, std::string const & path):
            dir(dir),
            path(path) {
        }

        ~DirInfo() {
            closedir(dir);
        }
    };

    static void addDirectoryToCrawl(DIR * dir, std::string const & path) {
        std::lock_guard<std::mutex> g(dirsAccess_);
        dirs_.push(new DirInfo(dir, path));
        dirsCV_.notify_one();
    }

    static DirInfo * getSomethingToCrawl() {
        std::unique_lock<std::mutex> guard(dirsAccess_);
        if (dirs_.empty()) {
            // decrement active threads counter and check if we are last, in which case terminate the execution
            if (--activeThreads_ == 0)
                throw "NOT_IMPLEMENTED";
            dirsCV_.wait(guard);
            // upon waking up increase active crawlers number
            ++activeThreads_;
        }
        DirInfo * result = dirs_.front();
        dirs_.pop();
        return result;
    }

    /** Returns the url of github project stored in given directory. If the directory is not a github project, returns an empty string.
      */
    std::string getProjectUrl(std::string const & path) {
        throw "NOT_IMPLEMENTED";
    }


    void processDirectory(DirInfo * dinfo) {
        std::string url = getProjectUrl(dinfo->path);
        // if the directory is not a prject, then append all its subdirectories to the queue
        if (url.empty()) {
            struct dirent * ent;
            while ((ent = readdir(dinfo->dir)) != nullptr) {
                // skip . and ..
                if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                    continue;
                // check if the thing we see is a directory and append it to the queue if so
                std::string path = dinfo->path + "/" + ent->d_name;
                DIR * d = opendir(path.c_str());
                if (d != nullptr)
                    addDirectoryToCrawl(d, path);
            }
        // otherwise create project and crawl it
        } else {
            GithubProject * project = new GithubProject(dinfo->path, url);
            // tokenize all files in the directory
            processProjectDirectory(project, dinfo->dir, dinfo->path, "");
            // close the project (it will be deleted either here, or when writer finishes last file
            project->close();
        }
        // finally, delete the directory info
        delete dinfo;
    }

    /** Project directory processor recursively crawls the directories in the same thread looking for language files to be tokenized.
     */
    void processProjectDirectory(GithubProject * project, DIR * d, std::string const & path, std::string const & relPath) {
        struct dirent * ent;
        while ((ent = readdir(d)) != nullptr) {
            // skip . and ..
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            // create relative and absolute paths
            std::string ap = path + "/" + ent->d_name;
            std::string rp = relPath.empty() ? ent->d_name : relPath + "/" + ent->d_name;
            // check if it is directory, in which case recurse
            DIR * d = opendir(ap.c_str());
            if (d != nullptr) {
                processProjectDirectory(project, d, ap, rp);
                // close the directory
                closedir(d);
            // if it is a file, and should be tokenized, process it
            } else if (isLanguageFile(ap)) {
                processFile(new TokenizedFile(project, rp));
            }
        }
    }

    /** Tokenizes the given file and submits it to the writer.
     */
    void processFile(TokenizedFile * f) {
        throw "NOT_IMPLEMENTED";
    }



    static std::mutex dirsAccess_;
    static std::queue<DirInfo *> dirs_;
    static std::condition_variable dirsCV_;


    /** Number of active crawler threads.

      When crawler has nothing to do, it suspends itself, decrementing this value. When the value reaches 0, all crawlers are suspended and the computation has finished.
     */
    static volatile unsigned activeThreads_;






};
