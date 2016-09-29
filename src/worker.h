#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>

#include "utils.h"

/** Basic worker thread.

 */
class Worker {
public:
    /** Simple statistics for each worker group.
     */
    struct Stats {
        /** Currently active threads, i.e. those not waiting for empty queue.
         */
        unsigned activeThreads;
        /** Current queue size.
         */
        unsigned queueSize;
        /** Jobs done so far.
         */
        unsigned jobsDone;

        /** Number of jobs that ended with errors.
         */
        unsigned errors;


        bool finished() {
            return queueSize == 0 and activeThreads == 0;
        }

        Stats(unsigned activeThreads, unsigned queueSize, unsigned jobsDone, unsigned errors):
            activeThreads(activeThreads),
            queueSize(queueSize),
            jobsDone(jobsDone),
            errors(errors) {
        }

        /** Simple pretty print.
         */
        friend std::ostream & operator << (std::ostream & s, Stats const & stats);

    };

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

    static std::mutex doneM_;
    static std::condition_variable allDone_;
    static std::atomic_uint numThreads_;

};

template<typename JOB>
class QueueWorker : public Worker {
public:
    static void Schedule(JOB const & job) {
        std::lock_guard<std::mutex> g(m_);
        jobs_.push(job);
        cv_.notify_one();
    }

    /** Run method of the thread.
     */
    void operator () () {
        Worker::Log("Started...");
        // bump up the number of active threads
        m_.lock();
        activate();
        m_.unlock();
        while (true) {
            JOB job = getJob();
            processAndCheck(job);
        }
    }

    /** Returns stats about current workers.
     */
    static Stats Statistic() {
        std::lock_guard<std::mutex> g(m_);
        return Stats(activeThreads_, jobs_.size(), jobsDone_, errors_);
    }
protected:
    QueueWorker(std::string const & name):
        Worker(name) {
    }

    /** Internal scheduler.

      Either appends given job to the queue, or utilizes the current thread to schedule it immediately.
     */
    void schedule(JOB const & job) {
        // TODO actually do the internal processing
        Schedule(job);
    }

    void activate() {
        Worker::activate();
        ++activeThreads_;
    }

    void deactivate() {
        --activeThreads_;
        Worker::deactivate();
    }


private:
    /** This is where all the magic happens.

      Override this in children to define worker's actual behavior.
     */
    virtual void process(JOB const & job) = 0;


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
        return result;
    }

    static unsigned activeThreads_;
    static unsigned jobsDone_;
    static unsigned errors_;
    static std::mutex m_;
    static std::condition_variable cv_;
    static std::queue<JOB> jobs_;
};

template<typename JOB>
unsigned QueueWorker<JOB>::activeThreads_ = 0;

template<typename JOB>
unsigned QueueWorker<JOB>::jobsDone_ = 0;

template<typename JOB>
unsigned QueueWorker<JOB>::errors_ = 0;

template<typename JOB>
std::mutex QueueWorker<JOB>::m_;

template<typename JOB>
std::condition_variable QueueWorker<JOB>::cv_;

template<typename JOB>
std::queue<JOB> QueueWorker<JOB>::jobs_;

template<typename JOB>
class QueueProcessor : public QueueWorker<JOB> {
public:

    static unsigned ProcessedFiles() {
        return processedFiles_;
    }

    static unsigned long ProcessedBytes() {
        return processedBytes_;
    }

    static double ProcessedMBytes() {
        return processedBytes_ / 1048576.0;
    }


protected:
    QueueProcessor(std::string const & name):
        QueueWorker<JOB>(name) {
    }

    static std::atomic_uint processedFiles_;
    static std::atomic_ulong processedBytes_;
};

template<typename JOB>
std::atomic_uint QueueProcessor<JOB>::processedFiles_;

template<typename JOB>
std::atomic_ulong QueueProcessor<JOB>::processedBytes_;

