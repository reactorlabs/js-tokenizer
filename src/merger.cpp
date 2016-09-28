#include <thread>

#include "merger.h"

void Merger::initializeWorkers(unsigned num) {
    for (unsigned i = 0; i < num; ++i) {
        std::thread t([i] () {
            Merger c(i);
            c();
        });
        t.detach();
    }
}




void Merger::process(MergerJob const & job) {


}
