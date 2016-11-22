#ifdef HAHA
#include <fstream>

#include "worker.h"
#include "tokenizer.h"


struct WriterJob {
    TokenizedFile * file;

    bool writeProject;

    unsigned originalPid;
    unsigned originalFid;

    bool isClone() const {
        return originalPid != 0 and originalFid != 0;
    }

    //static_assert(FILE_ID_STARTS_AT > 0, "0 means not initialized");
    //static_assert(PROJECT_ID_STARTS_AT > 0, "0 means not initialized");

/*    WriterJob(TokenizedFile * file):
        file(file),
        originalPid(0),
        originalFid(0) {
    } */

    WriterJob(TokenizedFile * file, bool writeProject, unsigned pid, unsigned fid):
        file(file),
        originalPid(pid),
        originalFid(fid),
        writeProject(writeProject) {
    }



    friend std::ostream & operator << (std::ostream & s, WriterJob const & job);
};

class Writer: public QueueProcessor<WriterJob> {
public:
    Writer(unsigned index);

    /** Initializes the given output directory.

      Makes sure all teh subdirs exist. If they do, reports a warning as data might be corrupted.
     */
    static void initializeOutputDirectory(std::string const & output);

    static void initializeWorkers(unsigned num);

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

#endif