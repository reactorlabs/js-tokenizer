#include "Writer.h"

std::string Writer::outputDir_;
std::mutex Writer::m_;
std::unordered_map<std::string, Writer::OutputFile *> Writer::outputFiles_;
