#pragma once

#include <vector>
#include <thread>
#include <dirent.h>


#include "project_crawler.h"

/** Tokenization driver.
 */
class Driver {
public:

    Driver(int argc, char * argv[]) {
        // this is here if we want to use commandline api in the future
        root_ = PATH_INPUT;
        output_ = PATH_OUTPUT;



        createOutputDirectories();

    }


    void run() {
        start_ = std::chrono::high_resolution_clock::now();
        std::mutex outputFilesMutex;
        std::vector<OutputFiles *> outputFiles(NUM_THREADS);
        // create output files
        for (size_t i = 0; i < NUM_THREADS; ++i)
            outputFiles[i] = new OutputFiles(output_, i);


        std::atomic_uint available_threads(NUM_THREADS);
        struct dirent * ent;
        DIR * root = opendir(root_.c_str());
        if (root == nullptr)
            throw STR("Unable to open projects root " << root_);
        std::cout << "Starting " << NUM_THREADS << " worker threads, projects root " << root_ << std::endl;
        while ((ent = readdir(root)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            std::string p = root_ + "/" + ent->d_name + "/latest";
            DIR * d = opendir(p.c_str());
            // it is a directory, wait for avaiable thread and schedule project crawler
            if (d != nullptr) {
                closedir(d);
                // busy wait until a thread becomes available
                while (available_threads == 0)
                    busyWait();
                // fire new thread
                --available_threads;
                // acquire output files in a lock
                outputFilesMutex.lock();
                OutputFiles * of = outputFiles.back();
                outputFiles.pop_back();
                outputFilesMutex.unlock();
                // run the thread
                std::thread t([& available_threads, & outputFiles, & outputFilesMutex, d, p, of]() {
                    ProjectCrawler crawler(p, *of);
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
        }
        closedir(root);
        while (available_threads < NUM_THREADS)
            busyWait();
        for (auto i : outputFiles)
            delete i;
        busyWait(true);
    }

    ~Driver() {
    }

private:

    void busyWait(bool final = false) {
        static unsigned lastDuration = 0;
        if (not final)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto now = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
        unsigned d = ms / 1000;
        if (d != lastDuration or final) {
            std::cout << "Processed files " << FileRecord::fileID(false) << ", throughput " << (total_bytes / (ms / 1000) / 1024 / 1024) << "[MB/s]" << std::endl;
            lastDuration = d;
        }
        if (final) {
            std::cout << "Total time:               " << (ms/1000) << " [s]" << std::endl;
            std::cout << "Total bytes:              " << (total_bytes / (double) 1024 / 1024) << "[MB]" << std::endl;
            std::cout << "Error files:              " << error_files << std::endl;
            std::cout << "Empty files:              " << empty_files << std::endl;

            std::cout << "Exact file matches:       " << exact_files << std::endl;
            std::cout << "Exact tokens matches:     " << exact_tokens << std::endl;


            long tf = FileRecord::fileID(false);
            std::cout << "Output files:             " << tf << std::endl;
#if SOURCERERCC_IGNORE_EMPTY_FILES == 1
            tf -= empty_files;
#endif
#if SOURCERERCC_IGNORE_FILE_HASH_EQUIVALENTS == 1
            tf -= exact_files;
#endif
#if SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS == 1
            tf -= exact_tokens;
#endif
            std::cout << "SourcererCC Output files: " << tf << std::endl;
        }

    }

    void createDirectory(std::string const & path) {
        if (system(STR("mkdir -p " << path).c_str()) != EXIT_SUCCESS)
            throw STR("Unable to create directory " << path);
    }

    void createOutputDirectories() {
        DIR * d = opendir(output_.c_str());
        if (d != nullptr) {
            closedir(d);
            std::cout << "!! Output directory already exists, id's may be not unique" << std::endl;
        }
        createDirectory(output_ + "/" + PATH_STATS_FILE);
        createDirectory(output_ + "/" + PATH_TOKENS_FILE);
        createDirectory(output_ + "/" + PATH_BOOKKEEPING_PROJS);
        createDirectory(output_ + "/" + PATH_OUR_DATA_FILE);
    }


    std::string root_;
    std::string output_;

    std::chrono::high_resolution_clock::time_point start_;



};
