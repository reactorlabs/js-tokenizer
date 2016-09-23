#include "writer.h"


std::mutex Writer::queueAccess_;
std::queue<TokenizedFile *> Writer::q_;
std::condition_variable Writer::qCV_;

std::map<std::string, unsigned> Writer::tokenHashMatches_;
std::map<std::string, unsigned> Writer::fileHashMatches_;

#if NUM_WRITERS > 1
std::mutex Writer::processingAccess_;
#endif

volatile unsigned long Writer::numFiles_ = 0;
volatile unsigned long Writer::numProjects_ = 0;
volatile unsigned long Writer::emptyFiles_ = 0;
volatile unsigned long Writer::tokenClones_ = 0;
volatile unsigned long Writer::fileClones_ = 0;
volatile unsigned long Writer::totalBytes_ = 0;

