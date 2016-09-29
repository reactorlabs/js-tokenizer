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
    Crawler::Schedule(CrawlerJob("/mnt/data1/data70k"));
    Crawler::Schedule(CrawlerJob("/mnt/data2/data70k2"));

    start = std::chrono::high_resolution_clock::now();

    Crawler::initializeWorkers(16);
    Tokenizer::initializeWorkers(16);
    Merger::initializeWorkers(4);
    Writer::initializeOutputDirectory("processed");
    Writer::initializeWorkers(1);
    do {
        displayStats(secondsSince(start));
    } while (not Worker::WaitForFinished(1000));
    displayStats(secondsSince(start));
    Worker::Log("ALL DONE");
    std::ofstream tokens("processed/tokens.txt");
    Merger::writeGlobalTokens(tokens);
}




int main(int argc, char * argv[]) {
    try {
        tokenize(argc, argv);
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
