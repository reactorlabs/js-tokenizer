#include "Downloader.h"



std::string Downloader::downloadDir_;

DBBuffer Downloader::projects_(DBWriter::TableProjects);
DBBuffer Downloader::projectsExtra_(DBWriter::TableProjectsExtra);

