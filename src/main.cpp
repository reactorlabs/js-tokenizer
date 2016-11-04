#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <thread>

#include "data.h"
#include "validator.h"
#include "crawler.h"
#include "tokenizer.h"
#include "merger.h"
#include "writer.h"

#include "escape_codes.h"

std::chrono::high_resolution_clock::time_point start;
std::chrono::high_resolution_clock::time_point end;




void help() {
    std::cout << "This should be helpful..." << std::endl;



}



void displayStats(double duration) {
    Worker::Stats c = Crawler::Statistic();
    Worker::Stats t = Tokenizer::Statistic();
    Worker::Stats m = Merger::Statistic();
    Worker::Stats w = Writer::Statistic();
    Worker::LockOutput();
    std::cout << eraseDown;
    std::cout << "Elapsed    " << time(duration) << " [h:mm:ss]" << std::endl << std::endl;

    std::cout << "Active threads " << Worker::NumActiveThreads() << std::endl;
    std::cout << "Crawler        " << c << std::endl;
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

    std::cout << "Unique tokens   " << Merger::NumUniqueTokens() << std::endl;
    std::cout << "Empty files     " << Merger::NumEmptyFiles() << pct(Merger::NumEmptyFiles(), Merger::ProcessedFiles()) << std::endl;
    std::cout << "Detected clones " << Merger::NumClones() << pct(Merger::NumClones(), Merger::ProcessedFiles()) << std::endl;
    std::cout << cursorUp(15);
    Worker::UnlockOutput();
}



void tokenize(int argc, char * argv[]) {
    if (argc < 2) {
        help();
        throw STR("Invalid number of arguments");
    }

    std::string outdir = argv[2];

    for (unsigned i = 3; i < argc; ++i)
        Crawler::Schedule(CrawlerJob(argv[i]));




    //Crawler::Schedule(CrawlerJob("/mnt/data1/data70k"));
    //Crawler::Schedule(CrawlerJob("/mnt/data2/data70k2"));

    start = std::chrono::high_resolution_clock::now();

    Crawler::SetQueueLimit(1000);
    Tokenizer::SetQueueLimit(1000);


    Crawler::initializeWorkers(2);
    Tokenizer::initializeWorkers(32);
    Merger::initializeWorkers(3);
    Writer::initializeOutputDirectory(outdir);
    Writer::initializeWorkers(1);

    do {
        displayStats(secondsSince(start));
    } while (not Worker::WaitForFinished(1000));

    displayStats(secondsSince(start));
    std::cout << cursorDown(15);
    Worker::Log("ALL DONE");
    std::ofstream tokens(STR(outdir << "/tokens.txt"));
    Merger::writeGlobalTokens(tokens);
}





/** Validates the tokenizer results.

  I.e. runs diffs on its clones where file hashes differ and checks that file hash same clones are byte for byte identical.
 */
void validate(int argc, char * argv[]) {
    std::cout << "Initializing..." << std::endl;
    Validator::Initialize("processed");
    start = std::chrono::high_resolution_clock::now();
    Validator::InitializeThreads(8);
    std::cout << "Initialized" << std::endl;
    do {
        Validator::DisplayStats(secondsSince(start));
    } while (not Worker::WaitForFinished(1000));
    Validator::DisplayStats(secondsSince(start));
    std::cout << cursorDown(10);


}




void process(int argc, char * argv[]) {

}

int main(int argc, char * argv[]) {
    try {
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
        //tokenize(argc, argv);
        //validate(argc, argv);
/*        FileStats::parseFile("/home/peta/delete/files-0.txt");
        std::cout << "Parsed " << FileStats::numFiles() << " files" << std::endl;

        CloneInfo::parseFile("/home/peta/delete/clones-0.txt");
        std::cout << "Parsed " << CloneInfo::numClones()<< " clone records" << std::endl;

        CloneGroup::find();
        std::cout << "Found " << CloneGroup::numGroups()<< " clone groups" << std::endl; */

//        auto x = Validator::validateExactClones();
//        std::cout << x.first / (double) CloneInfo::numClones() * 100 << std::endl;



        return EXIT_SUCCESS;
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
    } catch (char const * e) {
        std::cerr << e << std::endl;
    }

    return EXIT_FAILURE;

}

// project

// git rev-list --max-parents=0 HEAD | git log --stdin

// file

// git log --diff-filter=A -- src/utils.cpp


// add --pretty=format:"%at"

// to git log to get nice timestamp
