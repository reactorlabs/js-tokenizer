#pragma once

#include <dirent.h>
#include <cstring>

#include <queue>
#include <mutex>
#include <condition_variable>

#include "utils.h"
#include "data.h"
#include "writer.h"
#include "tokenizers/generic.h"
#include "tokenizers/js.h"
#include "worker.h"

/** Crawls the directory structures available and tokenizes files in projects.

  All crawlers share the queue of directories to check. When they reach a directory, they do the following:

  - check whether the directory is a valid github project
  - if there is no project, they start new crawler for each of its subdirectories


 */
class Crawler : public Worker {
public:
    Crawler(unsigned index):
        Worker(STR("CRAWLER " << index)) {
    }

    /** Adds directory to crawl during the startup phase.
     */
    static void addDirAndCheck(std::string const & path) {
        if (not isDirectory(path))
            throw STR("Unable to add directory " << path << " to crawler's queue");
        addDirectoryToCrawl(path);
    }

    void run() {
        Writer::print(this, "Started...");
        while (true) {
            try {
                std::string path = getSomethingToCrawl();
                processDirectory(path);
            } catch (std::string const & e) {
                Writer::error(this, e);
            } catch (std::exception const & e) {
                Writer::error(this, e.what());
            } catch (...) {
                Writer::error(this, "Unknown exception raised");
            }
        }
    }

    /** Returns the number of items in the crawler's queue.
     */
    static unsigned queueSize() {
        std::lock_guard<std::mutex> g(dirsAccess_);
        return dirs_.size();
    }

    /** Returns number of idling crawlers (i.e. threads waiting for some work to do.
     */
    static unsigned idleCrawlers() {
        return NUM_CRAWLERS - activeThreads_;
    }

private:

    static void addDirectoryToCrawl(std::string const & path) {
        std::lock_guard<std::mutex> g(dirsAccess_);
        dirs_.push(path);
        Worker::log(STR("Added crawl job  " << path << ", q size " << dirs_.size()));
        dirsCV_.notify_one();
    }

    static std::string getSomethingToCrawl() {
        std::unique_lock<std::mutex> guard(dirsAccess_);
        while (dirs_.empty()) {
            // decrement active threads counter and check if we are last, in which case terminate the execution
            if (--activeThreads_ == 0) {
                // release the lock, then exit calling the atexit handler
                dirsAccess_.unlock();
                exit(EXIT_SUCCESS);
            }
            dirsCV_.wait(guard);
            // upon waking up increase active crawlers number
            ++activeThreads_;
        }
        assert(dirs_.size() > 0);
        std::string result = dirs_.front();
        dirs_.pop();
        return result;
    }

    /** Returns the url of the git repository at given directory. Returns empty string if it is not a git repository.
      */
    std::string getProjectUrl(std::string const & path) {
        std::ifstream x(path + "/.git/config");
        // if not found, it is not git repository
        if (not x.good())
            return "";
        // parse the config file
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
                            return l;
                        }
                    }

                }
            }
        }
        return "";
    }

    void processDirectory(std::string const & dir) {
        Worker::log(this, STR("crawling directory " << dir));
        std::string url = getProjectUrl(dir);
        // if the directory is not a prject, then append all its subdirectories to the queue
        if (url.empty()) {
            Worker::log(this, STR("directory " << dir << " not recognized as git project, recursing..."));
            struct dirent * ent;
            DIR * d = opendir(dir.c_str());
            assert(d != nullptr);
            while ((ent = readdir(d)) != nullptr) {
                // skip . and ..
                if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                    continue;
#if IGNORE_GIT_FOLDER == 1
                // if the folder name is .git, do not crawl it
                if (strcmp(ent->d_name, ".git") == 0)
                    continue;
#endif
                // check if the thing we see is a directory and append it to the queue if so
                std::string path = dir + "/" + ent->d_name;
                if (isDirectory(path)) {
                    // depending on the size of the crawler's queue, either process the directory in this thread
                    if (queueSize() > CRAWLER_QUEUE_THRESHOLD) {
                        Worker::log(this, STR("Queue too large, recursing into " << path));
                        processDirectory(path);
                    // or go through the scheduling
                    } else {
                        Worker::log(this, STR("Adding job for directory " << path));
                        addDirectoryToCrawl(path);
                    }
                }
            }
            closedir(d);
        // otherwise create project and crawl it
        } else {
            Worker::log(this, STR("directory " << dir << " recognized as git project, tokenizing recursively"));
            GithubProject * project = new GithubProject(dir, url);
            // tokenize all files in the directory
            processProjectDirectory(project, dir, "");
            // close the project (it will be deleted either here, or when writer finishes last file
            project->close();
        }
    }

    /** Project directory processor recursively crawls the directories in the same thread looking for language files to be tokenized.
     */
    void processProjectDirectory(GithubProject * project, std::string const & path, std::string const & relPath) {
        Worker::log(this, STR("tokenizing source files in directory " << path));
        struct dirent * ent;
        DIR * d = opendir(path.c_str());
        while ((ent = readdir(d)) != nullptr) {
            // skip . and ..
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            // create relative and absolute paths
            std::string ap = path + "/" + ent->d_name;
            std::string rp = relPath.empty() ? ent->d_name : relPath + "/" + ent->d_name;
            // check if it is directory, in which case recurse
            if (isDirectory(ap))
                processProjectDirectory(project, ap, rp);
            // if it is a file, and should be tokenized, process it
            else if (isLanguageFile(ap))
                processFile(new TokenizedFile(project, rp));
        }
        closedir(d);
    }

    /** Tokenizes the given file and submits it to the writer.
     */
    void processFile(TokenizedFile * f) {
        Writer::log(this, STR("tokenizing file " << f->absPath()));
        JSTokenizer::tokenize(f);
        Writer::log(this, STR("submitting file " << f->absPath() << " to writers"));
        Writer::addTokenizedFile(f);
    }

    static std::mutex dirsAccess_;
    static std::queue<std::string> dirs_;
    static std::condition_variable dirsCV_;


    /** Number of active crawler threads.

      When crawler has nothing to do, it suspends itself, decrementing this value. When the value reaches 0, all crawlers are suspended and the computation has finished.
     */
    static volatile unsigned activeThreads_;






};
