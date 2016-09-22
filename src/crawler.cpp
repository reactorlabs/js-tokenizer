#include "crawler.h"

std::mutex Crawler::dirsAccess_;
std::queue<Crawler::DirInfo *> Crawler::dirs_;
volatile unsigned Crawler::activeThreads_ = NUM_THREADS;
