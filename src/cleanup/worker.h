#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>
#include <string>
#include <cassert>
#include <iostream>

class Thread {
public:

    struct Stats {
        std::string name;
        // number of threads started
        unsigned started;
        // number of threads currently idle
        unsigned idle;
        // number of threads currently stalled
        unsigned stalled;
        // current queue length
        size_t queueSize;
        // Number of jobs processed
        unsigned jobsDone;
        // Number of exceptions thrown
        unsigned errors;
        // Number of fatal errors (unknown exceptions)
        unsigned fatalErrors;
        // True if at least one thread has idled since last called
        bool hasIdled;
        // True if at least one thread has stalled since last called
        bool hasStalled;

        friend std::ostream & operator << (std::ostream & s, Stats const & stats);
    };

    static void Log(std::string const & what) {

    }

    static void Error(std::string const & what) {
        std::cerr << what;
        if (current_ != nullptr)
            std::cerr << " (" << current_->name_ << ")";
        std::cerr << std::endl;

    }

    static void Stall() {
        if (current_ != nullptr)
            current_->stall();
    }

    static void Resume() {
        if (current_ != nullptr)
            current_->resume();
    }

    template<typename W>
    static void InitializeWorkers(unsigned num) {
        static_assert(std::is_base_of<Thread, W>::value, "T must be subclass of Thread");
        for (unsigned i = 0; i < num; ++i) {
            std::thread t([i] () {
                try {
                    W c(i);
                    c();
                } catch (std::string const & e) {
                    Error(e);
                }
            });
            t.detach();
        }
    }

protected:
    Thread(std::string const & name, unsigned index):
        name_(name),
        index_(index) {
    }

private:
    template<typename JOB>
    friend class Worker;

    /** Called when the thread is about to be stalled */
    virtual void stall() {}

    /** Called when the thread has resumed from a stall. */
    virtual void resume() {}

    std::string name_;
    unsigned index_;

    static thread_local Thread * current_;
};



template<typename JOB>
class Worker : public Thread {
public:
    static Stats Statistics() {
        std::lock_guard<std::mutex> g(qm_);
        bool hasIdled = hasIdled_;
        bool hasStalled = hasStalled_;
        hasIdled_ = false;
        hasStalled_ = false;
        return Stats{className_, startedThreads_, idleThreads_, stalledThreads_, jobs_.size(), jobsDone_, errors_, fatalErrors_, hasIdled, hasStalled};
    }

    static void Schedule(JOB const & job) {
        std::unique_lock<std::mutex> g(qm_);
        while (queueFull()) {
            Stall();
            qcvAdd_.wait(g);
            Resume();
        }
        jobs_.push(job);
        qcv_.notify_one();
    }

    void operator () () {
        // set the current thread
        current_ = this;
        ++startedThreads_;
        Log("Started...");
        while (true) {
            getJob();
            try {
                process();
            } catch (std::string const & e) {
                ++errors_;
                Error(e);
            } catch (...) {
                ++fatalErrors_;
                Error("Fatal error");
            }
            ++jobsDone_;
        }
    }


protected:
    Worker(std::string const & className, unsigned index):
        Thread(className, index) {
        if (className_.empty())
            className_ = className;
        else
            assert(className_ == className);
    }

    JOB job_;

protected:
    // Errors (# of exceptions thrown by the process method)
    static std::atomic_uint errors_;
private:

    virtual void process() = 0;

    static bool queueFull() {
        return  (jobs_.size() >= queueMaxLength_);
    }

    virtual void stall() {
        ++stalledThreads_;
        hasStalled_ = true;
    }

    virtual void resume() {
        --stalledThreads_;
    }

    void getJob() {
        std::unique_lock<std::mutex> g(qm_);
        while (jobs_.empty()) {
            ++idleThreads_;
            hasIdled_ = true;
            qcv_.wait(g);
            --idleThreads_;
        }
        job_ = jobs_.front();
        jobs_.pop();
        if (not queueFull())
            qcvAdd_.notify_one();
    }


    // Name of the particular worker's class
    static std::string className_;
    // true if since last Statistics() call at least one thread has idled
    static bool hasIdled_;
    // true if since last Statistics() call at least one thread has stalled
    static bool hasStalled_;
    // Number of started threads (i.e. the max number of threads that can run
    static std::atomic_uint startedThreads_;
    // Number of idle threads (i.e. those waiting for new job)
    static std::atomic_uint idleThreads_;
    // Number of stalled threads
    static std::atomic_uint stalledThreads_;
    // Number of jobs processed (including errors)
    static std::atomic_uint jobsDone_;
    // Fatal errors, i.e. the number of unrecognized exceptions thrown from the process method
    static std::atomic_uint fatalErrors_;

    // Queue access mutex
    static std::mutex qm_;

    // Queue size condition variable
    static std::condition_variable qcv_;

    // Conditional variable for dependent threads to wait on
    static std::condition_variable qcvAdd_;

    static std::queue<JOB> jobs_;


    // Maximum length of the jobs queue. When the queue is larger than this number the thread scheduling next job will be paused
    static unsigned queueMaxLength_;
};


template<typename J>
std::string Worker<J>::className_ = "";

template<typename J>
bool Worker<J>::hasIdled_;

template<typename J>
bool Worker<J>::hasStalled_;

template<typename J>
std::atomic_uint Worker<J>::startedThreads_(0);

template<typename J>
std::atomic_uint Worker<J>::idleThreads_(0);

template<typename J>
std::atomic_uint Worker<J>::stalledThreads_(0);

template<typename J>
std::atomic_uint Worker<J>::jobsDone_(0);

template<typename J>
std::atomic_uint Worker<J>::errors_(0);

template<typename J>
std::atomic_uint Worker<J>::fatalErrors_(0);

template<typename J>
std::mutex Worker<J>::qm_;

template<typename J>
std::condition_variable Worker<J>::qcv_;

template<typename J>
std::condition_variable Worker<J>::qcvAdd_;

template<typename J>
std::queue<J> Worker<J>::jobs_;

template<typename J>
unsigned Worker<J>::queueMaxLength_(1000);




