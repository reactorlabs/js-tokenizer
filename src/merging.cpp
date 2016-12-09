#include "worker.h"
#include "buffers.h"
#include "workers/ResultsMerger.h"


template<typename T>
void initializeThreads(unsigned num) {
    Thread::InitializeWorkers<T>(num);
    Thread::Print(STR("    " <<  T::Name() << " (" << num << ")" << std::endl));
}

void mergeResults() {
    initializeThreads<ResultsMerger>(1);
    ResultsMerger::Schedule(ResultsMerger::Job(0, 99, TokenizerKind::Generic));
    while (true) {

    }
}
