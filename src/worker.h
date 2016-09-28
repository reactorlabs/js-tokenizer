#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

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
protected:
    /** Each worker must have a name.
     */
    Worker(std::string const & name):
        name_(name) {
    }

    /* Locked output routines for different kinds of messages when the worker thread's name is known.
     */
    void error(std::string const & msg);
    void warning(std::string const & msg);
    void print(std::string const & msg);
    void log(std::string const & msg);

protected:
    /** True if current job raised an error
     */
    bool error_;

private:
    std::string name_;


    static std::mutex m_;

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
        log("Started...");
        // bump up the number of active threads
        m_.lock();
        ++activeThreads_;
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

private:
    /** This is where all the magic happens.

      Override this in children to define worker's actual behavior.
     */
    virtual void process(JOB const & job) = 0;


    /** Processes the job and checks for errors, updating the counters.

      Calls the process() method, manages the error countres & state and checks for any errors the processing might throw so that the thread won't die.
     */
    void processAndCheck(JOB const & job) {
        bool oldError = error_;
        error_ = false;
        try {
            process(job);
        } catch (std::string const & e) {
            error(STR(e << " while doing job " << job));
            error_ = true;
        } catch (std::exception const & e) {
            error(STR(e.what() << "while doing job " << job));
            error_ = true;
        } catch (...) {
            error(STR("Unknown exception while doing job " << job));
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
            --activeThreads_;
            cv_.wait(g);
            ++activeThreads_;
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

