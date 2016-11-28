#include "CSVReader.h"

std::string CSVReader::language_;

std::atomic_uint CSVReader::totalProjects_(0);
std::atomic_uint CSVReader::languageProjects_(0);
std::atomic_uint CSVReader::forkedProjects_(0);
std::atomic_uint CSVReader::deletedProjects_(0);
std::atomic_uint CSVReader::validProjects_(0);
