#include "data.h"
#include "workers/DBWriter.h"
#include "workers/Writer.h"

bool ClonedProject::keepProjects_ = false;

unsigned ClonedProject::stride_count_ = 1;
unsigned ClonedProject::stride_index_ = 0;



std::atomic_uint TokenizedFile::idCounter_(0);
std::atomic_uint TokenInfo::idCounter_(0);



