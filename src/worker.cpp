#include <iostream>
#include <iomanip>

#include "worker.h"

std::mutex Worker::m_;

bool Worker::stalled_ = false;

thread_local Worker * Worker::worker_ = nullptr;

std::mutex Worker::doneM_;
std::condition_variable Worker::allDone_;
std::atomic_uint Worker::numThreads_;

//std::cout << "Worker                  Status  T/Active %   Queue  Done       Errors %   Files    Bytes " << std::endl;
//std::cout << "----------------------- ------- -------- --- ------ ---------- ------ --- ---------" << std::endl;
//                                      STALLED                                           999999999

std::ostream & operator << (std::ostream & s, QueueWorkerStats const & stats) {
    s << std::setw(24) << std::left << stats.name;
    std::string status = "        ";
    if (stats.stalled)
        status = "STALLED ";
    else if (stats.active == 0)
        status = "IDLE    ";
    s << status;
    s << std::setw(8) << std::left << STR(stats.started << "/" << stats.active);
    s << std::setw(4) << std::right << pct(stats.active,stats.started);
    s << std::setw(7) << stats.queue;
    s << std::setw(11) << stats.done;
    s << std::setw(7) << stats.errors;
    s << std::setw(4) << pct(stats.errors, stats.done);
    if (stats.files > 0)
        s << std::setw(10) << stats.files;
    else
        s << "          ";
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
    return;
    std::lock_guard<std::mutex> g(m_);
    std::cout << "LOG: " << msg;
    std::string name = currentName();
    if (not name.empty())
        std::cout << " (" << name << ")" << std::endl;
    else
        std::cout << std::endl;
}

bool Worker::WaitForFinished(int timeoutMillis) {
    if (numThreads_ == 0)
        return true;
    while (timeoutMillis > 0) {
        auto start = std::chrono::high_resolution_clock::now();
        std::unique_lock<std::mutex> g(doneM_);
        allDone_.wait_for(g, std::chrono::milliseconds(timeoutMillis));
        timeoutMillis -= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        if (numThreads_ == 0)
            return true;
    }
    return false;

}
