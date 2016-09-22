#include "writer.h"


std::mutex Writer::queueAccess_;
std::queue<TokenizedFile *> Writer::q_;
std::condition_variable Writer::qCV_;
