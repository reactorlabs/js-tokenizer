#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <thread>

#include "data.h"
#include "validator.h"
#include "CSVParser.h"
#include "downloader.h"
#include "crawler.h"
#include "tokenizer.h"
#include "merger.h"
#include "writer.h"

#include "escape_codes.h"

std::chrono::high_resolution_clock::time_point start;
std::chrono::high_resolution_clock::time_point end;




void help() {
    std::cout << "Tokenizer & Downloader" << std::endl;
    std::cout << "----------------------" << std::endl << std::endl;
    std::cout << "Usage:" << std::endl << std::endl;
    std::cout << "tokenizer ACTION {COMMON_ARGS} {ACTION_ARGS}" << std::endl << std::endl;
    std::cout << "Where COMMON_ARGS stands for the following" << std::endl << std::endl;

    std::cout << "-v | --verbose     ---   enables verbose output" << std::endl;
    std::cout << "-dt=N              ---   specifies number of downloader threads" << std::endl;
    std::cout << "-ct=N              ---   specifies number of crawler threads" << std::endl;
    std::cout << "-tt=N              ---   specifies number of tokenier threads" << std::endl;
    std::cout << "-mt=N              ---   specifies number of merger threads" << std::endl;
    std::cout << "-wt=N              ---   specifies number of writer threads" << std::endl;

    std::cout << "-dq=N              ---   specifies max size of downloader job queue (and opened projects)" << std::endl;
    std::cout << "-cq=N              ---   specifies max size of crawler job queue" << std::endl;
    std::cout << "-tq=N              ---   specifies max size of tokenizer queue" << std::endl;
    std::cout << "-mq=N              ---   specifies max size of merger queue (and opened files)" << std::endl;
    std::cout << "-wq=N              ---   specifies max size of writer queue (and opened files)" << std::endl;

    std::cout << "-tt=js             ---   enables only the JavaScript specific tokenizer" << std::endl;
    std::cout << "-tt=js+generic     ---   enables the generic and the JavaScript specific tokenizer" << std::endl;


    std::cout << "And ACTION & corresponding ACTION_ARGS stand for:" << std::endl << std::endl;
    std::cout << "-t | --tokenize    ---   tokenizes all github projects in given folders" << std::endl;
    std::cout << "-d | --download    ---   downloads github projects stored in the specified repos and tokenies them" << std::endl;

    std::cout << "-v | --validate    ---   validates results previous obtained" << std::endl;


}

void parseArguments(int argc, char * argv[]) {
    // ignore the first argument, which



}



void displayStats(double duration) {
    Worker::Stats c = Crawler::Statistic();
    Worker::Stats d = Downloader::Statistic();
    Worker::Stats t = Tokenizer::Statistic();
    Worker::Stats m = Merger::Statistic();
    Worker::Stats w = Writer::Statistic();
    Worker::LockOutput();
    std::cout << eraseDown;
    std::cout << "Elapsed    " << time(duration) << " [h:mm:ss]" << std::endl << std::endl;

    std::cout << "Active threads " << Worker::NumActiveThreads() << std::endl;
    std::cout << "Crawler        " << c << std::endl;
    std::cout << "Downloader     " << d << std::endl;
    std::cout << "Tokenizer      " << t << std::endl;
    std::cout << "Merger         " << m << std::endl;
    std::cout << "Writer         " << w << std::endl << std::endl;

    std::cout << "Files      "
              << std::setw(8) << Tokenizer::ProcessedFiles() << " tokenizer"
              << std::setw(8) << Merger::ProcessedFiles() << " merger"
              << std::setw(8) << Writer::ProcessedFiles() << " writer"
              << std::endl;

    std::cout << "Bytes      " << std::setprecision(2) << std::fixed
              << std::setw(8) << Tokenizer::ProcessedMBytes() << " tokenizer"
              << std::setw(8) << Merger::ProcessedMBytes() << " merger"
              << std::setw(8) << Writer::ProcessedMBytes() << " writer [MB]"
              << std::endl;

    std::cout << "Throughput "
              << std::setw(8) << (Tokenizer::ProcessedMBytes() / duration) << " tokenizer"
              << std::setw(8) << (Merger::ProcessedMBytes() / duration) << " merger"
              << std::setw(8) << (Writer::ProcessedMBytes() / duration) << " writer [MB/s]"
              << std::endl << std::endl;

    std::cout << "Unique tokens     " << Merger::NumUniqueTokens() << std::endl;
    std::cout << "Empty files       " << Merger::NumEmptyFiles() << pct(Merger::NumEmptyFiles(), Merger::ProcessedFiles()) << std::endl;
    std::cout << "Detected clones   " << Merger::NumClones() << pct(Merger::NumClones(), Merger::ProcessedFiles()) << std::endl;
    std::cout << "JS errors         " << Tokenizer::jsErrors() << pct(Tokenizer::jsErrors(), Tokenizer::ProcessedFiles()) << std::endl;
    std::cout << cursorUp(16);
    Worker::UnlockOutput();
}



void tokenize(int argc, char * argv[]) {
    if (argc < 2) {
        help();
        throw STR("Invalid number of arguments");
    }

    Crawler::SetQueueLimit(10000);
    Tokenizer::SetQueueLimit(10000);
    Merger::SetQueueLimit(10000);
    Writer::SetQueueLimit(10000);


    std::string outdir = argv[2];

    for (unsigned i = 3; i < argc; ++i)
        Crawler::Schedule(CrawlerJob(argv[i]), nullptr);

    start = std::chrono::high_resolution_clock::now();


    Writer::initializeOutputDirectory(outdir);


    Worker::InitializeThreads<Crawler>(8);
    Worker::InitializeThreads<Tokenizer>(8);
    Worker::InitializeThreads<Merger>(8);
    Worker::InitializeThreads<Writer>(1);

    do {
        displayStats(secondsSince(start));
    } while (not Worker::WaitForFinished(1000));

    displayStats(secondsSince(start));
    std::cout << cursorDown(15);
    Worker::Log("ALL DONE");
    std::ofstream tokens(STR(outdir << "/tokens.txt"));
    Merger::writeGlobalTokens(tokens);
}

void downloaderStatistics() {
    Worker::LockOutput();
    std::cout << "Total projects " << CSVParser::TotalProjects() << ", language " << CSVParser::LanguageProjects() << ", deleted " << CSVParser::DeletedProjects() << ", forked " << CSVParser::ForkedProjects() << ", valid " << CSVParser::ValidProjects() << std::endl;
    Worker::UnlockOutput();
}

/** Download the stuff from the repo and tokenize it on the fly.
 */
void download(int argc, char * argv[]) {
    CSVParser::Schedule("/home/peta/sourcerer/projects.small.csv", nullptr);
    CSVParser::SetLanguage("JavaScript");

    Downloader::SetKeepWhenDone(true);
    Downloader::SetDownloadDir("/home/peta/sourcerer/downloaded");
    Downloader::Initialize();


    start = std::chrono::high_resolution_clock::now();
    Worker::InitializeThreads<CSVParser>(1);
    Worker::InitializeThreads<Downloader>(1);
    Worker::InitializeThreads<Tokenizer>(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    do {
        displayStats(secondsSince(start));
    } while (not Worker::WaitForFinished(1000));

}





/** Validates the tokenizer results.

  I.e. runs diffs on its clones where file hashes differ and checks that file hash same clones are byte for byte identical.
 */
void validate(int argc, char * argv[]) {
#ifdef HAHA

    std::cout << "Initializing..." << std::endl;
    Validator::Initialize("processed");
    start = std::chrono::high_resolution_clock::now();
    Validator::InitializeThreads(8);
    std::cout << "Initialized" << std::endl;
    do {
        //Validator::DisplayStats(secondsSince(start));
    } while (not Worker::WaitForFinished(1000));
    Validator::DisplayStats(secondsSince(start));
    std::cout << cursorDown(10);
#endif

}




void process(int argc, char * argv[]) {

}
/** Changes that I must implement:

  - instead of taking data from disk, take data from the csv file
  - download each project, i.e. have github project management
  - the downloader can load up to N projects at the same time
  - change the output of large tokens so that we hash them when necessary
  - use database to check for file hashes
 */






int main(int argc, char * argv[]) {
    try {
        download(argc, argv);
        return EXIT_SUCCESS;


        if (argc < 2) {
            help();
            throw STR("Invalid number of arguments");
        }
        std::string cmd = argv[1];
        if (cmd == "help" or cmd == "--help" or cmd == "-h") {
            help();
        } else if (cmd == "tokenize" or cmd == "--tokenize" or cmd == "-t") {
            tokenize(argc, argv);
        } else if (cmd == "validate" or cmd == "--validate" or cmd == "-v") {
            validate(argc, argv);
        } else if (cmd == "process" or cmd == "--process" or cmd == "-p") {
            process(argc, argv);
        } else {
            help();
            throw STR("Invalid command " << cmd);
        }
        return EXIT_SUCCESS;
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
    } catch (char const * e) {
        std::cerr << e << std::endl;
    }
    return EXIT_FAILURE;

}

