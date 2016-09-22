#include "driver.h"

namespace {

    std::mutex outputFilesMutex;
    std::vector<OutputFiles *> outputFiles(NUM_THREADS);
    std::atomic_uint available_threads(NUM_THREADS);
}


void Driver::run() {
    start_ = std::chrono::high_resolution_clock::now();
    // create output files
    for (size_t i = 0; i < NUM_THREADS; ++i)
        outputFiles[i] = new OutputFiles(output_, i);
    std::cout << "Starting " << NUM_THREADS << " worker threads" << std::endl;
    for (std::string const & rootPath : root_) {
        DIR * root = opendir(rootPath.c_str());
        if (root == nullptr)
            throw STR("Unable to open projects root " << rootPath);
        crawlDirectory(root, rootPath);
    }
    while (available_threads < NUM_THREADS)
        busyWait();
    for (auto i : outputFiles)
        delete i;
    busyWait(true);
}

/** Extracts the project github url.

  Goes into the `latest/.git/config`, locates the `[remote "origin"]` section and fetches the `url` property.

  This is very crude, but might just work nicely. Another
 */
std::string Driver::gitUrl(std::string const & path) {
    std::ifstream x(path + "/.git/config");
    if (not x.good()) {
        return "";
    }
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
    return "";
}

std::string Driver::convertGitUrlToHTML(std::string url) {
    if (url.substr(url.size() - 4) != ".git")
        return "";
    if (url.substr(0, 15) == "git@github.com:") {
        url = url.substr(15, url.size() - 19);
    } else if (url.substr(0, 17) == "git://github.com/") {
        url = url.substr(17, url.size() - 21);
    } else if (url.substr(0, 19) == "https://github.com/") {
        url = url.substr(19, url.size() - 23);
    } else {
        return "";
    }
    return "https://github.com/" + url;
}

void Driver::crawlDirectory(DIR * dir, std::string const & path) {
    // first see if the directory itself is a github repo
    std::string url = gitUrl(path);
    if (not url.empty()) {
        crawlProject(path, url);
    // if not, crawl recursivery if something beneath is github repo
    } else {
        struct dirent * ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            std::string p = path + "/" + ent->d_name;
            DIR * d = opendir(p.c_str());
            // it is a directory, wait for avaiable thread and schedule project crawler
            if (d != nullptr)
                crawlDirectory(d, p);
        }
    }
    closedir(dir);
}

void Driver::crawlProject(std::string const & path, std::string const & url) {
    while (available_threads == 0)
        busyWait();
    --available_threads;
    // acquire output files in a lock
    outputFilesMutex.lock();
    OutputFiles * of = outputFiles.back();
    outputFiles.pop_back();
    outputFilesMutex.unlock();
    // run the thread
    std::thread t([path, url, of]() {
        ProjectCrawler crawler(path, url, *of);
        crawler.crawl();
        // return output threads
        outputFilesMutex.lock();
        outputFiles.push_back(of);
        outputFilesMutex.unlock();
        // free thread's resources
        ++available_threads;
    });
    t.detach();
}

