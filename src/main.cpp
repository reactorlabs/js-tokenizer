#ifndef __x86_64__
#error "Must use 64 bit system!"
#endif

#include <iomanip>
#include <thread>

#include "data.h"
#include "worker.h"

#include "workers/CSVReader.h"

// TODO tokenizer might add number of total files to the prject info and so on

// TODO Perhaps even remove the queue restrictions because we do not really care that much about them, they contain only pointers really

// TODO this should support multiple languages too (through the use of isLanguageFile, which wemight do better

// js and generic files have different id's this is not something I would want

// DBWriter should flush its buffers when it goes to idle...

// Projects do not delete themselves

// make sure tokens and clone groups are as small as possible

// output tokens and clone groups at the end, and ideally do this multithreaded

// TODO convert assertions to errors

// TODO make errors print current job

// TODO produce a database stamp at the end

void addTokenizer(TokenizerKind k) {
    Tokenizer::AddTokenizer(k);
    Merger::AddTokenizer(k);
    DBWriter::AddTokenizer(k);
    Writer::AddTokenizer(k);
}

template<typename T>
void reportLivingObjects(std::string const & name) {
    std::cout << "  " << std::setw(50) << std::left << name ;
    std::cout << std::setw(12) << std::right << T::Instances();
    std::cout << std::setw(4) << pct(T::Instances(), T::MaxInstances());
    std::cout << std::endl;
}

std::string time(double secs) {
    unsigned seconds = (unsigned) secs;

    unsigned h = seconds / 3600;
    seconds = seconds % 3600;
    unsigned m = seconds / 60;
    unsigned s = seconds % 60;
    return STR(std::setfill('0') << std::setw(2) << h << ":" << std::setfill('0') << std::setw(2) << m << ":" << std::setfill('0') << std::setw(2) << s);
}


/** Waits for a second, then displays statistics. Returns true if all jobs have been finished and all threads are idle, false otherwise. */
bool stats(std::chrono::high_resolution_clock::time_point const & since) {



    // sleep for a second - this is not super precise but at the time it takes to run we do not care about such precission anyways...
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto now = std::chrono::high_resolution_clock::now();
    double secondsSince = std::chrono::duration_cast<std::chrono::milliseconds>(now - since).count() / 1000.0;

    // get all statistics
    std::vector<Thread::Stats> s;
    s.push_back(CSVReader::Statistics());
    s.push_back(Downloader::Statistics());
    s.push_back(Tokenizer::Statistics());
    s.push_back(Merger::Statistics());
    s.push_back(DBWriter::Statistics());
    s.push_back(Writer::Statistics());

    // get other info
    unsigned long projects = s[2].jobsDone;
    unsigned long files = Tokenizer::TotalFiles();
    unsigned long bytes = Tokenizer::TotalBytes();

    // a very simple heuristic where we have assumed projects to have
    unsigned EXPECTED_PROJECTS = s[1].queueSize + s[1].jobsDone;
    if (EXPECTED_PROJECTS < 2300000)
        EXPECTED_PROJECTS = 2300000;

    unsigned memory = 0;
    memory += sizeof(ClonedProject) * ClonedProject::Instances();
    memory += sizeof(TokenizedFile) * TokenizedFile::Instances();
    memory += sizeof(TokensMap) * TokensMap::Instances();
    memory += sizeof(CloneGroup) * Merger::NumCloneGroups();
    memory += sizeof(CloneGroup) * Merger::NumTokens();
    memory += sizeof(Hash) * Merger::UniqueFileHashes();
    memory += sizeof(std::string) * s[0].queueSize;
    memory += sizeof(DownloaderJob) * s[1].queueSize;
    memory += sizeof(TokenizerJob) * s[2].queueSize;
    memory += sizeof(MergerJob) * s[3].queueSize;
    memory += sizeof(DBWriterJob) * s[4].queueSize;
    memory += sizeof(WriterJob) * s[5].queueSize;

    double secondsTotal = secondsSince * EXPECTED_PROJECTS / projects;


    Thread::LockOutput();
    std::cout << "Statistics                                                     #   %" << std::endl;
    std::cout << "--------------------------------------------------- ------------ ---" << std::endl;
    std::cout << "  Elapsed time                                     " << std::setw(12) << time(secondsSince) << std::setw(4) << pct(secondsSince, secondsTotal) << std::endl;
    std::cout << "    Estimated remaining time                       " << std::setw(12) << time(secondsTotal - secondsSince) << std::endl;
    std::cout << "  Projects                                         " << std::setw(12) << projects << std::setw(4) << pct(Tokenizer::JobsDone(), EXPECTED_PROJECTS) << std::endl;
    std::cout << "  Files                                            " << std::setw(12) << files << std::endl;
    std::cout << "    JS Errors                                      " << std::setw(12) << JavaScriptTokenizer::ErrorFiles() << std::setw(4) << pct(JavaScriptTokenizer::ErrorFiles(), files);

    // general progress information
    std::cout << "Speed                                                              /s" << std::endl;
    std::cout << "------------------------------------------------ ---------- ---------" << std::endl;
    std::cout << "  Files                                         " << std::setw(11) << files << std::setw(10) << round(files / secondsSince, 2) << std::endl;
    std::cout << "  Bytes                                         " << std::setw(11) <<xbytes(bytes) << std::setw(10) << xbytes(bytes / secondsSince) << std::endl;
    std::cout << std::endl;

    // Worker statistics
    std::cout << "Worker                      Up   I*   %  S*    %  QSize       Done   Err Fatal   %" << std::endl;
    std::cout << "-------------------------- --- ---  --- ---  --- ------ ---------- ----- ----- ---" << std::endl;
    for (Thread::Stats & stats : s)
        //if (stats.started > 0)
            std::cout << "  " << stats << std::endl;
    std::cout << std::endl;

    // Memory statistics
    std::cout << "Living objects                                                 #   %" <<  std::endl;
    std::cout << "--------------------------------------------------- ------------ ---" << std::endl;
    reportLivingObjects<ClonedProject>("Projects");
    reportLivingObjects<TokenizedFile>("Files");
    reportLivingObjects<TokensMap>("Token Maps");
    std::cout << "  Unique file hashes                               " << std::setw(12) << Merger::UniqueFileHashes() << std::endl;
    std::cout << "  Generic" << std::endl;
    std::cout << "    Clone groups objects                           " << std::setw(12) << Merger::NumCloneGroups(TokenizerKind::Generic) << std::endl;
    std::cout << "    Token info objects                             " << std::setw(12) << Merger::NumTokens(TokenizerKind::Generic) << std::endl;
    std::cout << "  JavaScript" << std::endl;
    std::cout << "    Clone groups objects                           " << std::setw(12) << Merger::NumCloneGroups(TokenizerKind::JavaScript) << std::endl;
    std::cout << "    Token info objects                             " << std::setw(12) << Merger::NumTokens(TokenizerKind::JavaScript) << std::endl;
    std::cout << std::endl;
    std::cout << "  ASSUMED MEMORY USE (without overhead)            " << std::setw(12) << xbytes(memory) << std::setw(4) << pct(memory, ((unsigned)64) * 1024 * 1024 * 1024) << std::endl;
    std::cout << std::endl;

    Thread::UnlockOutput();

    for (Thread::Stats & stats : s)
        if (stats.queueSize > 0 or stats.idle != stats.started)
            return false;
    return true;
}



int main(int argc, char * argv[]) {

    // let's make this artificially high so that DB has time to rest, let the # of objects do the job
    DBWriter::SetQueueMaxLength(100000);

    CSVReader::SetLanguage("JavaScript");

    Downloader::SetDownloadDir("/data/sourcerer/github/download");
    DBWriter::SetConnection("127.0.0.1", "bigdata", "sourcerer", "js");
    Writer::SetOutputDir("/data/sourcerer/github/output");

    addTokenizer(TokenizerKind::Generic);
    addTokenizer(TokenizerKind::JavaScript);

    CSVReader::Schedule("/home/peta/sourcerer/projects.small.csv");



    Thread::InitializeWorkers<DBWriter>(4);
    Thread::InitializeWorkers<CSVReader>(1);
    Thread::InitializeWorkers<Downloader>(50);
    Thread::InitializeWorkers<Tokenizer>(4);
    Thread::InitializeWorkers<Merger>(2);
    Thread::InitializeWorkers<Writer>(1);

    auto start = std::chrono::high_resolution_clock::now();
    // wait for the tokenizer to finish
    while (not stats(start)) { }
    // flush merger stats and wait for all the jobs to finish
    Merger::FlushStatistics();
    while (not stats(start)) { }
    // finally

    return EXIT_SUCCESS;
}
