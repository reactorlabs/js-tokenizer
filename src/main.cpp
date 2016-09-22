#include <cstdlib>

#include "config.h"
#include "globals.h"
#include "driver.h"



// Global statistics
std::atomic_ulong total_bytes;
std::atomic_ulong empty_files;
std::atomic_ulong error_files;
std::atomic_ulong exact_files;
std::atomic_ulong exact_tokens;

// mutex for accessing global data
std::mutex m;

// hashmap for exact matches in tokens
std::map<std::string, unsigned> exactTokenMatches;
// hashmap for exact matches in files
std::map<std::string, unsigned> exactFileMatches;























int main(int argc, char * argv[]) {
    try {
        FileTokenizer::initializeLanguage();
        Driver d(argc, argv);
        d.run();
        return EXIT_SUCCESS;
    } catch (std::string const & msg) {
        std::cerr << msg << std::endl;
        return EXIT_FAILURE;
    }





}
