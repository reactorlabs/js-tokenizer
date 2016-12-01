#include <ostream>
#include <iomanip>


#include "worker.h"

#include "helpers.h"

std::ostream & operator << (std::ostream & s, Thread::Stats const & stats) {
    s << std::setw(24) << std::left << stats.name << std::right
      << std::setw(4) << stats.started
      << std::setw(4) << stats.idle << std::setw(1) << (stats.hasIdled ? "*" : " ")
      << std::setw(4) << pct(stats.idle, stats.started)
      << std::setw(4) << stats.stalled << std::setw(1) << (stats.hasStalled ? "!" : " ")
      << std::setw(4) << pct(stats.stalled, stats.started)
      << std::setw(7) << stats.queueSize
      << std::setw(11) << stats.jobsDone
      << std::setw(6) << stats.errors
      << std::setw(6) << stats.fatalErrors
      << std::setw(4) << pct(stats.errors + stats.fatalErrors, stats.jobsDone);
    if (stats.idle == stats.started)
        s << " IDLE";
    else if (stats.stalled == stats.started)
        s << " STALLED";
    return s;
}

thread_local Thread * Thread::current_;

std::mutex Thread::out_;
std::ofstream Thread::logfile_;
