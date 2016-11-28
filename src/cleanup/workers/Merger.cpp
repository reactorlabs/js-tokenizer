#include "Merger.h"

std::mutex Merger::mFileHash_;
std::unordered_set<Hash> Merger::uniqueFileHashes_;
std::vector<Merger::Context *> Merger::contexts_;
