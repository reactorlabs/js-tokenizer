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

std::chrono::high_resolution_clock::time_point start;
std::chrono::high_resolution_clock::time_point end;


void tokenize(int argc, char * argv[]) {
    Crawler::Schedule(CrawlerJob("/home/peta/sourcerer/data/jakub2"));

    start = std::chrono::high_resolution_clock::now();

    Crawler::initializeWorkers(2);
    Tokenizer::initializeWorkers(4);
    Merger::initializeWorkers(1);
    Writer::initializeOutputDirectory("processed");
    Writer::initializeWorkers(1);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Worker::Stats c = Crawler::Statistic();
        Worker::Stats t = Tokenizer::Statistic();
        Worker::Stats m = Merger::Statistic();
        Worker::Stats w = Writer::Statistic();
        double s = secondsSince(start);
        std::cout << "Elapsed    " << time(s) << " [h:mm:ss]" << std::endl << std::endl;

        std::cout << "Crawler    " << c << std::endl;
        std::cout << "Tokenizer  " << t << std::endl;
        std::cout << "Merger     " << m << std::endl;
        std::cout << "Writer     " << w << std::endl << std::endl;

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
                  << std::setw(8) << (Tokenizer::ProcessedMBytes() / s) << " tokenizer"
                  << std::setw(8) << (Merger::ProcessedMBytes() / s) << " merger"
                  << std::setw(8) << (Writer::ProcessedMBytes() / s) << " writer [MB/s]"
                  << std::endl;


        if (w.finished() and m.finished() and t.finished() and c.finished())
            break;
    }
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
