#include <iostream>
#include <iomanip>

#include "worker.h"

std::mutex Worker::m_;


std::ostream & operator << (std::ostream & s, Worker::Stats const & stats) {
    s << "active " << std::setw(3) << stats.activeThreads
      << "queue " << std::setw(8) << stats.queueSize
      << "done " << std::setw(10) << stats.jobsDone
      << "errors " << std::setw(8) << stats.errors;
    return s;
}





void Worker::Error(std::string const & msg) {
   std::lock_guard<std::mutex> g(m_);
   std::cerr << "ERROR: " << msg << std::endl;
}

void Worker::Warning(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cerr << "WARNING: " << msg << std::endl;
}

void Worker::Print(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cout << msg << std::endl;
}

void Worker::Log(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cout << "LOG: " << msg << std::endl;
}

void Worker::error(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cerr << "ERROR: " << msg << " (" << name_ << ")" << std::endl;
    error_ = true;
}

void Worker::warning(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cerr << "WARNING: " << msg << " (" << name_ << ")" << std::endl;

}

void Worker::print(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cout <<  msg << " (" << name_ << ")" << std::endl;

}

void Worker::log(std::string const & msg) {
    std::lock_guard<std::mutex> g(m_);
    std::cout << "LOG: " << msg << " (" << name_ << ")" << std::endl;
}

