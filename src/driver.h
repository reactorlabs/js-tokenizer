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
        output_ = PATH_OUTPUT;

#define GENERATE_ROOT_ENTRY(PATH) root_.push_back(PATH);
PATH_INPUT(GENERATE_ROOT_ENTRY)



        createOutputDirectories();

    }


    void run();

    ~Driver() {
    }

private:

    std::string gitUrl(std::string const & path);

    std::string convertGitUrlToHTML(std::string url);

    void crawlDirectory(DIR * d, std::string const & path);

    void crawlProject(std::string const & path, std::string const & url);



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


    std::vector<std::string> root_;
    std::string output_;

    std::chrono::high_resolution_clock::time_point start_;



};
