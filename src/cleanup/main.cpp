#include "data.h"
#include "worker.h"

#include "workers/CSVReader.h"




void stats() {
    Thread::Stats reader = CSVReader::Statistics();



}


int main(int argc, char * argv[]) {
    Thread::InitializeWorkers<CSVReader>(1);

    return EXIT_SUCCESS;
}
