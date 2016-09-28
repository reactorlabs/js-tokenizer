#ifdef HAHA
#include "crawler.h"

std::mutex Crawler::dirsAccess_;
std::queue<Crawler::Job> Crawler::dirs_;
std::condition_variable Crawler::dirsCV_;
volatile unsigned Crawler::activeThreads_ = NUM_CRAWLERS;


#endif
