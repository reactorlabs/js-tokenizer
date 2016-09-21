#pragma once

#include <atomic>
#include <mutex>
#include <map>
#include <string>

// Global statistics
extern std::atomic_ulong total_bytes;
extern std::atomic_ulong empty_files;
extern std::atomic_ulong error_files;
extern std::atomic_ulong exact_files;
extern std::atomic_ulong exact_tokens;

// mutex for accessing global data
extern std::mutex m;

// hashmap for exact matches in tokens
extern std::map<std::string, unsigned> exactTokenMatches;
// hashmap for exact matches in files
extern std::map<std::string, unsigned> exactFileMatches;

