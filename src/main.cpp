#include <cstdlib>
#include <iostream>

#include "data.h"
#include "validator.h"



int main(int argc, char * argv[]) {
    try {
        FileStatistic::parseFile("/home/peta/delete/files-0.txt");
        std::cout << "Parsed " << FileStatistic::numFiles() << " files" << std::endl;
        CloneInfo::parseFile("/home/peta/delete/clones-0.txt");
        std::cout << "Parsed " << CloneInfo::numClones()<< " clone records" << std::endl;

        auto x = Validator::validateExactClones();
        std::cout << x.first / (double) CloneInfo::numClones() * 100 << std::endl;

        return EXIT_SUCCESS;
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
    } catch (char const * e) {
        std::cerr << e << std::endl;
    }
    return EXIT_FAILURE;

}
