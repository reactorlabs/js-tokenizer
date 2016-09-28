#include <fstream>

#include "worker.h"
#include "data.h"


struct WriterJob {
    TokenizedFile * file;

    bool isClone() const {

    }


    WriterJob(TokenizedFile * file):
        file(file) {
    }

    friend std::ostream & operator << (std::ostream & s, WriterJob const & job);
};

class Writer: public QueueWorker<WriterJob> {
public:
    Writer(unsigned index);

    /** Initializes the given output directory.

      Makes sure all teh subdirs exist. If they do, reports a warning as data might be corrupted.
     */
    static void initializeOutputDirectory(std::string const & output);

private:

    static void openStreamAndCheck(std::ofstream & s, std::string const & filename);

    /** Writer just outputs the information stored into the respective output streams.
     */
    void process(WriterJob const & job) override;

    std::ofstream files_;
    std::ofstream projs_;
    std::ofstream tokens_;
    std::ofstream clones_;
    std::ofstream fullStats_;

    static std::string outputDir_;

};

