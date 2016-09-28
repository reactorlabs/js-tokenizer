#include "worker.h"


struct CrawlerJob {
    std::string path;

    CrawlerJob(std::string const & path):
        path(path) {
    }

    friend std::ostream & operator << (std::ostream & s, CrawlerJob const & job) {
        s << job.path;
        return s;
    }
};

class Crawler : public QueueWorker<CrawlerJob> {
public:
    Crawler(unsigned index):
        QueueWorker<CrawlerJob>(STR("CRAWLER " << index)) {
    }

private:
    /** Checks whether a directory is git project, rercusively.
     */
    void process(CrawlerJob const & job) override;

    /** Returns the URL of git project in the given directory, or empty string if the directory is not git project. */
    static std::string projectUrl(std::string const & path);
};
