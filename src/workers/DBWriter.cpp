#include "DBWriter.h"

std::string DBWriter::host_;
std::string DBWriter::user_;
std::string DBWriter::pass_;
std::string DBWriter::db_;
std::mutex DBWriter::m_;

