#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>
#include <thread>

#include "utils.h"

/** Basic worker thread.

 */
class Worker {
public:

    /* Locked output routines for different kinds of messages.
     */
    static void Error(std::string const & msg);
    static void Warning(std::string const & msg);
    static void Print(std::string const & msg);
    static void Log(std::string const & msg);

    static std::string currentName() {
        if (worker_ == nullptr)
            return "";
        else
            return worker_->name_;
    }

    static void LockOutput() {
        m_.lock();
    }

    static void UnlockOutput() {
        m_.unlock();
    }

    static bool WaitForFinished(int timeoutMillis);

    static unsigned NumActiveThreads() {
        return numThreads_;
    }

    template<typename T>
    static void InitializeThreads(unsigned num) {
        static_assert(std::is_base_of<Worker, T>::value, "T must be subclass of Worker");
        for (unsigned i = 0; i < num; ++i) {
            std::thread t([i] () {
                T c(i);
                c();
            });
            t.detach();
        }
    }

    void stall() {
        stalled_ = true;
    }

protected:


    /** Each worker must have a name.
     */
    Worker(std::string const & name):
        name_(name) {
        if (worker_ != nullptr)
            throw STR("Worker " << worker_->name_ << " already running in thread attempting to create worker " << name);
        worker_ = this;
    }

    void activate() {
        ++numThreads_;
    }

    void deactivate() {
        if (--numThreads_ == 0) {
            std::lock_guard<std::mutex> g(doneM_);
            allDone_.notify_all();
        };
    }

protected:
    /** True if current job raised an error
     */
    bool error_;

private:
    std::string name_;

    static thread_local Worker * worker_;

    static std::mutex m_;

    static bool stalled_;

    static std::mutex doneM_;
    static std::condition_variable allDone_;
    static std::atomic_uint numThreads_;

};


class QueueWorkerStats {
public:
    std::string name;
    unsigned started;
    unsigned active;
    unsigned queue;
    unsigned done;
    bool stalled;
    unsigned errors;
    unsigned files;
    unsigned long bytes;

    QueueWorkerStats(std::string name, unsigned started, unsigned active, unsigned queue, unsigned done, bool stalled, unsigned errors, unsigned files, unsigned bytes):
        name(name),
        started(started),
        active(active),
        queue(queue),
        done(done),
        stalled(stalled),
        errors(errors),
        files(files),
        bytes(bytes) {
    }

};

std::ostream & operator << (std::ostream & s, QueueWorkerStats const & stats);






template<typename JOB>
class QueueWorker : public Worker {
public:
    /** External scheduling.
        std::unique_lock<std::mutex> g(m_);
        while (jobs_.empty()) {
            deactivate();
            cv_.wait(g);
            activate();
        }
        JOB result = jobs_.front();
        jobs_.pop();
        return result;

      Blocks if the queue is too large and only adds the request when done.
     */
    static void Schedule(JOB const & job, Worker * sender) {
        std::unique_lock<std::mutex> g(m_);
        while (jobs_.size() > queueLimit_) {
            sender->stall();
            canAdd_.wait(g);
        }
        jobs_.push(job);
        cv_.notify_one();
    }

    /** Run method of the thread.
     */
    void operator () () {
        ++startedThreads_;
        Worker::Log("Started...");
        while (true) {
            JOB job = getJob();
            processAndCheck(job);
        }
    }

    /** Returns stats about current workers.
     */
    static QueueWorkerStats Statistic() {
        std::lock_guard<std::mutex> g(m_);
        bool stalled = stalled_;
        stalled_ = false;
        return QueueWorkerStats(className_, startedThreads_,activeThreads_,jobs_.size(),jobsDone_,stalled, errors_,processedFiles_,processedBytes_);
    }

    static unsigned QueueLength() {
        std::lock_guard<std::mutex> g(m_);
        return jobs_.size();
    }

    static void SetQueueLimit(unsigned limit) {
        queueLimit_ = limit;
    }

protected:
    friend class QueueWorkerStats;

    QueueWorker(std::string const & name, unsigned index):
        Worker(STR(name << " " << index)) {
        // bump up the number of active threads
        m_.lock();
        activate();
        className_ = name;
        m_.unlock();

    }

    /** Internal scheduler.

      Either appends given job to the queue, or utilizes the current thread to schedule it immediately.
     */
    void schedule(JOB const & job) {
        if (queueLimit_ == 0 or jobs_.size() >= queueLimit_)
            processAndCheck(job);
        else
            Schedule(job, this);
    }

    void activate() {
        Worker::activate();
        ++activeThreads_;
    }

    void deactivate() {
        --activeThreads_;
        Worker::deactivate();
    }

    /** Processes the job and checks for errors, updating the counters.

      Calls the process() method, manages the error countres & state and checks for any errors the processing might throw so that the thread won't die.
     */
    void processAndCheck(JOB const & job) {
        Worker::Log(STR(job));
        bool oldError = error_;
        error_ = false;
        try {
            process(job);
        } catch (std::string const & e) {
            Worker::Error(STR(e << " while doing job " << job));
            error_ = true;
        } catch (std::exception const & e) {
            Worker::Error(STR(e.what() << "while doing job " << job));
            error_ = true;
        } catch (...) {
            Worker::Error(STR("Unknown exception while doing job " << job));
            error_ = true;
        }
        ++jobsDone_;
        // if error was reported, increase total number of errors
        if (error_)
            ++errors_;
        // return the error indicator for the previous job
        error_ = oldError;
    }

    /** This is where all the magic happens.

      Override this in children to define worker's actual behavior.
     */
    virtual void process(JOB const & job) = 0;

    /** Gets new piece of job from the queue, or blocks the thread if empty.
     */
    JOB getJob() {
        std::unique_lock<std::mutex> g(m_);
        while (jobs_.empty()) {
            deactivate();
            cv_.wait(g);
            activate();
        }
        JOB result = jobs_.front();
        jobs_.pop();
        if (jobs_.size() < queueLimit_)
            canAdd_.notify_one();
        return result;
    }

    static std::atomic_uint startedThreads_;
    static unsigned activeThreads_;
    static unsigned jobsDone_;
    static unsigned errors_;
    static std::atomic_uint processedFiles_;
    static std::atomic_ulong processedBytes_;
    static std::mutex m_;
    static std::condition_variable cv_;
    static std::condition_variable canAdd_;
    static std::queue<JOB> jobs_;

    static std::string className_;

    static unsigned queueLimit_;
};

template<typename JOB>
std::atomic_uint QueueWorker<JOB>::startedThreads_(0);

template<typename JOB>
unsigned QueueWorker<JOB>::activeThreads_ = 0;

template<typename JOB>
unsigned QueueWorker<JOB>::jobsDone_ = 0;

template<typename JOB>
unsigned QueueWorker<JOB>::errors_ = 0;

template<typename JOB>
std::atomic_uint QueueWorker<JOB>::processedFiles_;

template<typename JOB>
std::atomic_ulong QueueWorker<JOB>::processedBytes_;

template<typename JOB>
std::mutex QueueWorker<JOB>::m_;

template<typename JOB>
std::condition_variable QueueWorker<JOB>::cv_;

template<typename JOB>
std::condition_variable QueueWorker<JOB>::canAdd_;

template<typename JOB>
std::queue<JOB> QueueWorker<JOB>::jobs_;

template<typename JOB>
unsigned QueueWorker<JOB>::queueLimit_ = 1000;

template<typename JOB>
std::string QueueWorker<JOB>::className_ = "???";


