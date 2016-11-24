#include <ostream>
#include <iomanip>

#include "worker.h"


std::ostream & operator << (std::ostream & s, Thread::Stats const & stats) {
    s << std::setw(24) << std::left << stats.name <<
         stats.started <<
         stats.idle <<
         stats.stalled <<
         stats.queueSize <<
         stats.jobsDone <<
         stats.errors <<
         stats.fatalErrors <<
         stats.hasIdled <<
         stats.hasStalled;
}

thread_local Thread * Thread::current_;
