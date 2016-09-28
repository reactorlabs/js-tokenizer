#include <cstdlib>
#include <iostream>

#include "data.h"
#include "validator.h"
#include "crawler.h"
#include "tokenizer.h"


void tokenize(int argc, char * argv[]) {
    Crawler::Schedule(CrawlerJob("/home/peta/sourcerer/data"));

    Crawler::initializeWorkers(1);
    Tokenizer::initializeWorkers(1);
//    Writer::initializeWorkers(1);

    while (true) {

    }
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
