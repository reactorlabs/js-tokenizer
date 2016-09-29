#include <iostream>
#include <iomanip>

#include "worker.h"

std::mutex Worker::m_;

thread_local Worker * Worker::worker_ = nullptr;


std::ostream & operator << (std::ostream & s, Worker::Stats const & stats) {
    s << "active " << std::setw(2) << stats.activeThreads
      << " queue " << std::setw(8) << stats.queueSize
      << " done " << std::setw(9) << stats.jobsDone
      << " errors " << std::setw(0) << stats.errors;
    return s;
}





void Worker::Error(std::string const & msg) {
   std::lock_guard<std::mutex> g(m_);
   std::cerr << "ERROR: " << msg;
   std::string name = currentName();
   if (not name.empty())
       std::cerr << " (" << name << ")" << std::endl;
   else
       std::cerr << std::endl;
}

void Worker::Warning(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cerr << "WARNING: " << msg;
    std::string name = currentName();
    if (not name.empty())
        std::cerr << " (" << name << ")" << std::endl;
    else
        std::cerr << std::endl;
}

void Worker::Print(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cout << msg;
    std::string name = currentName();
    if (not name.empty())
        std::cout << " (" << name << ")" << std::endl;
    else
        std::cout << std::endl;
}

void Worker::Log(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cout << "LOG: " << msg;
    std::string name = currentName();
    if (not name.empty())
        std::cout << " (" << name << ")" << std::endl;
    else
        std::cout << std::endl;
}
