#include <iomanip>
#include <thread>

#include "data.h"
#include "worker.h"

#include "workers/CSVReader.h"

// TODO tokenizer might add number of total files to the prject info and so on

// TODO Perhaps even remove the queue restrictions because we do not really care that much about them, they contain only pointers really

// TODO this should support multiple languages too (through the use of isLanguageFile, which wemight do better

void addTokenizer(TokenizerKind k) {
    Tokenizer::AddTokenizer(k);
    Merger::AddTokenizer(k);
    DBWriter::AddTokenizer(k);
    Writer::AddTokenizer(k);
}

template<typename T>
void reportLivingObjects(std::string const & name) {
    std::cout << "  " << std::setw(50) << std::left << name ;
    std::cout << std::setw(10) << std::right << T::Instances();
    std::cout << std::setw(4) << pct(T::Instances(), T::MaxInstances());
    std::cout << std::endl;
}


/** Waits for a second, then displays statistics. Returns true if all jobs have been finished and all threads are idle, false otherwise. */
bool stats() {
    std::this_thread::sleep_for(std::chrono::seconds(1));


    // get all statistics
    std::vector<Thread::Stats> s;
    s.push_back(CSVReader::Statistics());
    s.push_back(Downloader::Statistics());
    s.push_back(Tokenizer::Statistics());
    s.push_back(Merger::Statistics());
    s.push_back(DBWriter::Statistics());
    s.push_back(Writer::Statistics());

    std::cout << "Worker                      Up   I*   %  S*    %  QSize       Done   Err Fatal" << std::endl;
    std::cout << "-------------------------- --- ---  --- ---  --- ------ ---------- ----- -----" << std::endl;


    for (Thread::Stats & stats : s)
        //if (stats.started > 0)
            std::cout << "  " << stats << std::endl;

    std::cout << std::endl;

    // and now write extra stuff
    std::cout << "Living objects                                               #   %" <<  std::endl;
    std::cout << "--------------------------------------------------- ---------- ---" << std::endl;
    reportLivingObjects<ClonedProject>("Projects");
    reportLivingObjects<TokenizedFile>("Files");
    reportLivingObjects<TokensMap>("Token Maps");

    std::cout << std::endl;






    for (Thread::Stats & stats : s)
        if (stats.queueSize > 0 or stats.idle != stats.started)
            return false;
    return true;
}



int main(int argc, char * argv[]) {

    CSVReader::Schedule("somecsvtoread");

    Downloader::SetDownloadDir("/home/peta/sourcerer/download");
    DBWriter::SetConnection("localhost", "bigdata", "sourcerer", "js");
    Writer::SetOutputDir("/home/peta/sourcerer/output");

    addTokenizer(TokenizerKind::Generic);
    addTokenizer(TokenizerKind::JavaScript);

    Thread::InitializeWorkers<CSVReader>(1);
    Thread::InitializeWorkers<Downloader>(1);
    Thread::InitializeWorkers<Tokenizer>(1);
    Thread::InitializeWorkers<Merger>(1);
    Thread::InitializeWorkers<DBWriter>(1);
    Thread::InitializeWorkers<Writer>(1);

    stats();

    return EXIT_SUCCESS;
}
