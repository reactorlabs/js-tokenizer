#include <cstdlib>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>



#include "config.h"
#include "worker.h"
#include "crawler.h"
#include "writer.h"
#include "tokenizers/generic.h"




std::chrono::high_resolution_clock::time_point start;









/** Populates the crawler's queue with input directories.
 */
void initializeCrawlerQueue() {
    Writer::print(STR("Initializing crawler's queue..."));
#define ADD_CRAWLER_Q_ITEM(PATH) Crawler::addDirAndCheck(PATH);
    PATH_INPUT(ADD_CRAWLER_Q_ITEM);
    Writer::print(STR("    " << Crawler::queueSize() << " items"));
}

/** Initializes the writer threads and output directories.
 */
void initializeWriters() {
    // create output directories
    DIR * d = opendir(PATH_OUTPUT);
    if (d != nullptr) {
        closedir(d);
        Writer::error(STR("Output directory " << PATH_OUTPUT << " already exists, data may be corrupted, but continuing..."));
    }
    createDirectory(STR(PATH_OUTPUT << "/" << PATH_STATS_FILE));
    createDirectory(STR(PATH_OUTPUT << "/" << PATH_TOKENS_FILE));
    createDirectory(STR(PATH_OUTPUT << "/" << PATH_BOOKKEEPING_PROJS));
    createDirectory(STR(PATH_OUTPUT << "/" << PATH_OUR_DATA_FILE));
    // launch writer threads
    for (size_t i = 0; i < NUM_WRITERS; ++i) {
        std::thread t([i] () {
            try {
                Writer w(i);
                w.run();
            } catch (std::string const & e) {
                Worker::error(e);
                exit(EXIT_FAILURE);
            }
        });
        t.detach();
    }
    Writer::print(STR("Created " << NUM_WRITERS << " writer threads"));
}

/** Spawns the required ammount of crawler threads and starts the tokenization.
 */
void initializeCrawlers() {
    for (size_t i = 0; i < NUM_CRAWLERS; ++i) {
        std::thread t([i] () {
            Crawler c(i);
            c.run();
        });
        t.detach();
    }
    Writer::print(STR("Created " << NUM_CRAWLERS << " crawler threads"));
}

/** Nice time printer.
 */
std::string time(double sec) {
    unsigned s = static_cast<unsigned>(sec);
    s = s % 60;
    unsigned m = s / 60;
    m = m % 60;
    unsigned h = m / 60;
    return STR(h << ":" << std::setfill('0') << std::setw(2) << m << ":" << s);
}

void report() {
    unsigned long processedFiles = Writer::processedFiles();
    double processedMB = Writer::processedBytes() / 1024.0 / 1024 ;
    unsigned pendingJobs = Crawler::queueSize();
    unsigned pendingFiles = Writer::queueSize();
    auto now = std::chrono::high_resolution_clock::now();
    double s = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0;
    Worker::print(STR("Duration " << time(s) << "[h:m:s] : Processed " << processedFiles << " files, " << processedMB << " MB; throughput " << (processedMB / s) << "[MB/s]; pending " << pendingJobs << " jobs, " << pendingFiles << " writes."));
}

void done() {
    Worker::print("CRAWLERS finished, waiting for writers");
    while (Writer::queueSize() != 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Worker::print("DONE");
    report();
    auto now = std::chrono::high_resolution_clock::now();
    double s = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0;
    Worker::print("---");
    Worker::print(STR("Total time:            " << time(s) << " [h:m:s]"));
    Worker::print(STR("Projects processed:    " << Writer::processedProjects()));
    Worker::print(STR("Files processed:       " << Writer::processedFiles()));
    Worker::print(STR("Total size:            " << Writer::processedBytes() / 1024.0 / 1024 << "[MB]"));
    double tf = Writer::processedFiles() / 100.0;
    Worker::print(STR("Empty files:           " << Writer::emptyFiles() << (SOURCERERCC_IGNORE_EMPTY_FILES ? " (skipped)" : "")));
    Worker::print(STR("                       " << Writer::emptyFiles() / tf << " %"));
    Worker::print(STR("Detected token clones: " << Writer::tokenClones() << (SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS ? " (skipped)" : "")));
    Worker::print(STR("                       " << Writer::tokenClones() / tf << " %"));
    Worker::print(STR("Detected file clones:  " << Writer::fileClones() << (SOURCERERCC_IGNORE_FILE_HASH_EQUIVALENTS ? " (skipped)" : "")));
    Worker::print(STR("                       " << Writer::fileClones() / tf << " %"));
}



int main(int argc, char * argv[]) {
    // set the atexit event - this prints out the final information
    std::atexit(done);
    try {
        Tokenizer::initializeLanguage();
        initializeCrawlerQueue();
        initializeWriters();
        start = std::chrono::high_resolution_clock::now();
        initializeCrawlers();
        // report periodically
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            report();
        }
    } catch (std::string const & e) {
        Worker::error(e);
    } catch (std::exception const & e) {
        Worker::error(e.what());
    } catch (...) {
        Worker::error("Execution terminated after throwing an unknown error");
    }

    return EXIT_FAILURE;
}
