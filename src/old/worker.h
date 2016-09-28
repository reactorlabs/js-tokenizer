#pragma once

#include <iostream>
#include <mutex>

#include "config.h"

#error "here "

class Worker {
public:

    static void log(std::string const & what) {
#if VERBOSE == 1
        std::lock_guard<std::mutex> g(output_);
        std::cout << what << std::endl;
#endif

    }

    static void log(Worker const * sender, std::string const & what) {
#if VERBOSE == 1
        std::lock_guard<std::mutex> g(output_);
        std::cout << what << " (" << sender->name() << ")" << std::endl;
#endif
    }

    static void print(std::string const & what) {
        std::lock_guard<std::mutex> g(output_);
        std::cout << what << std::endl;

    }

    static void print(Worker const * sender, std::string const & what) {
        std::lock_guard<std::mutex> g(output_);
        std::cout << what << " (" << sender->name() << ")" << std::endl;

    }

    static void error(std::string const & what) {
        std::lock_guard<std::mutex> g(output_);
        std::cerr << "ERROR: " << what << std::endl;

    }

    static void error(Worker const * sender, std::string const & what) {
        std::lock_guard<std::mutex> g(output_);
        std::cerr << "ERROR: " << what << " (" << sender->name() << ")" << std::endl;
    }

    /** Returns the name of the worker thread.
     */
    std::string const & name() const {
        return name_;
    }


protected:
    Worker(std::string const & name):
        name_(name) {
    }

private:

    std::string name_;

    /** cout and cerr guard. */
    static std::mutex output_;

};
