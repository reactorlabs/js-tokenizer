#ifndef __x86_64__
#error "Must use 64 bit system!"
#endif

#include <iomanip>
#include <thread>

#include "data.h"
#include "buffers.h"
#include "worker.h"

#include "workers/CSVReader.h"

/**

# Tokenization

Projects, files and file extras are simple enough as they will never collide among strides so they go immediately to the database.

Stats also go to the database, but here we must do insert ignore. Even if we tokenie the file twice, because the stats are id'd only on the file hash it does not matter as it will always be the same.

Everything else has to be merged and therefore goes to files:

clonepairs (id and id)
clonegroups (id is file id, hash of the group is based on the tokens hash of that file)


# Merging

Once the strides finish, merging o




*/


int lastStride = -1;


void tokenize(std::string const & filename) {
    std::ifstream s(filename);
    s.seekg(0, std::ios::end);
    unsigned long resizeSize = s.tellg();
    std::string data;
    data.resize(resizeSize);
    s.seekg(0, std::ios::beg);
    s.read(& data[0], data.size());
    s.close();
    std::shared_ptr<TokenizedFile> g(new TokenizedFile(1, nullptr, "haha", 67));
    std::shared_ptr<TokenizedFile> js(new TokenizedFile(1, nullptr, "haha", 67));
    GenericTokenizer t(data, g);
    JavaScriptTokenizer t2(data, js);
    t.tokenize();
    //t2.tokenize();
    std::cout << "generic: " << g->lines << " -- " << g->loc << " -- " << g->sloc << std::endl;
    std::cout << "js:      " << js->lines << " -- " << js->loc << " -- " << js->sloc << std::endl;
}




void addTokenizer(TokenizerKind k) {
    Tokenizer::AddTokenizer(k);
    Merger::AddTokenizer(k);
}

template<typename T>
std::string reportLivingObjects(std::string const & name) {
    return STR("  " << std::setw(50) << std::left << name  <<
        std::setw(12) << std::right << T::Instances() <<
        std::setw(4) << pct(T::Instances(), T::MaxInstances()) << std::endl);
}

std::string time(double secs) {
    unsigned seconds = (unsigned) secs;

    unsigned d = seconds / (3600 * 24);
    seconds = seconds % (3600 * 24);

    unsigned h = seconds / 3600;
    seconds = seconds % 3600;
    unsigned m = seconds / 60;
    unsigned s = seconds % 60;
    return STR(d << ":" << std::setfill('0') << std::setw(2) << h << ":" << std::setfill('0') << std::setw(2) << m << ":" << std::setfill('0') << std::setw(2) << s);
}

template<typename T>
std::string reportTokenizerFileStats(unsigned totalFiles) {
    unsigned errors = T::ErrorFiles();
    unsigned fileHashes = Merger::UniqueFileHashes(T::kind);
    unsigned tokenHashes = Merger::UniqueTokenHashes(T::kind);

    std::stringstream ss;
    ss << "  " << T::kind << std::endl;
    ss << "    Errors                                         " << std::setw(14) << errors << std::setw(4) << pct(errors, totalFiles) << std::endl;
    ss << "    Unique file hashes                             " << std::setw(14) << fileHashes << std::setw(4) << pct(fileHashes, totalFiles) << std::endl;
    ss << "    Unique token hashes                            " << std::setw(14) << tokenHashes << std::setw(4) << pct(tokenHashes, totalFiles) << std::endl;
    return ss.str();
}

/** Waits for a second, then displays statistics. Returns true if all jobs have been finished and all threads are idle, false otherwise. */
bool stats(std::string & output, std::chrono::high_resolution_clock::time_point const & since) {
    std::stringstream ss;


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

    // adjust for the stride calculation
    EXPECTED_PROJECTS /= ClonedProject::StrideCount();

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
    memory += (1024 * 1024) * s[4].queueSize; // these are large buffers
    memory += sizeof(WriterJob) * s[5].queueSize;

    // add downloader errors to the # of projects
    double secondsTotal = secondsSince * EXPECTED_PROJECTS / (s[1].errors + projects);
    // Worker statistics
    ss << "Worker                      Up   I*   %  S*    %  QSize       Done   Err Fatal   %" << std::endl;
    ss << "-------------------------- --- ---  --- ---  --- ------ ---------- ----- ----- ---" << std::endl;
    for (Thread::Stats & stats : s)
        //if (stats.started > 0)
            ss << "  " << stats << std::endl;
    ss << std::endl;

    ss << "Statistics                                                     #   %" << std::endl;
    ss << "--------------------------------------------------- ------------ ---" << std::endl;
    ss << "  Elapsed time                                     " << std::setw(14) << time(secondsSince) << std::setw(4) << pct(secondsSince, secondsTotal) << std::endl;
    ss << "    Estimated remaining time                       " << std::setw(14) << time(secondsTotal - secondsSince) << std::endl;
    ss << "  Projects                                         " << std::setw(14) << projects << std::setw(4) << pct(Tokenizer::JobsDone(), EXPECTED_PROJECTS) << std::endl;
    ss << "  Files                                            " << std::setw(14) << files << std::endl;
    ss << reportTokenizerFileStats<GenericTokenizer>(files);
    ss << reportTokenizerFileStats<JavaScriptTokenizer>(files);


    // general progress information
    ss << "Speed                                                              /s" << std::endl;
    ss << "------------------------------------------------ ---------- ---------" << std::endl;
    ss << "  Files                                         " << std::setw(11) << files << std::setw(10) << round(files / secondsSince, 2) << std::endl;
    ss << "  Bytes                                         " << std::setw(11) <<xbytes(bytes) << std::setw(10) << xbytes(bytes / secondsSince) << std::endl;
    ss << std::endl;


    // Memory statistics
    ss << "Living objects                                                 #   %" <<  std::endl;
    ss << "--------------------------------------------------- ------------ ---" << std::endl;
    ss << reportLivingObjects<ClonedProject>("Projects");
    ss << reportLivingObjects<TokenizedFile>("Files");
    ss << reportLivingObjects<TokensMap>("Token Maps");
//    ss << "  Unique file hashes                                " << std::setw(12) << Merger::UniqueFileHashes() << std::endl;
    ss << "  Generic" << std::endl;
    ss << "    Clone groups objects                            " << std::setw(12) << Merger::NumCloneGroups(TokenizerKind::Generic) << std::endl;
    ss << "    Token info objects                              " << std::setw(12) << Merger::NumTokens(TokenizerKind::Generic) << std::endl;
    ss << "  JavaScript" << std::endl;
    ss << "    Clone groups objects                            " << std::setw(12) << Merger::NumCloneGroups(TokenizerKind::JavaScript) << std::endl;
    ss << "    Token info objects                              " << std::setw(12) << Merger::NumTokens(TokenizerKind::JavaScript) << std::endl;
    ss << std::endl;
    ss << "  ASSUMED MEMORY USE (without overhead)             " << std::setw(12) << xbytes(memory) << std::setw(4) << pct(memory / 1024, 64 * 1024 * 1024) << std::endl;
    ss << std::endl;

    output = ss.str();

    for (Thread::Stats & stats : s)
        if (stats.queueSize > 0 or stats.idle != stats.started)
            return false;
    return true;
}


void resumeState(SQLConnection & sql, TokenizerKind t) {
#ifdef HAHA
    Thread::Print(STR("  resuming previous state for tokenizer " << " TODO NAME " << std::endl));
    std::string p = prefix(t);

    std::string tableStats = STR(p << DBWriter::TableStats);
    std::string tableFilesExtra = STR(p << DBWriter::TableFilesExtra);
    std::string tableCloneGroups = STR(p << DBWriter::TableCloneGroups << "_" << lastStride);
    std::string tableCloneGroupHashes = STR(p << DBWriter::TableCloneGroupHashes << "_" << lastStride);
    std::string tableTokens = STR(p << DBWriter::TableTokens << "_" << lastStride);
    std::string tableTokenHashes = STR(p << DBWriter::TableTokenHashes << "_" << lastStride);


    // first get unique hashes, these are just simply all hashes in stats so far
    Thread::Print(STR("    unique file hashes..." << std::endl));
    unsigned count = 0;
    sql.query(STR("SELECT fileHash FROM " << tableStats), [t, & count] (unsigned cols, char ** row) {
       if (++count % 1000000 == 0)
           Thread::Print(STR("      " << count << std::endl), false);
       Merger::AddUniqueFileHash(t, Hash::Parse(row[0]));
   });
    Thread::Print(STR("      total: " <<  Merger::UniqueFileHashes(t) << std::endl));

    // get clone groups, each clone group has to be joined with file extra information so that we have oldest date *and* file id
    Thread::Print(STR("    clone groups..." << std::endl));
    count = 0;
    sql.query(STR("SELECT groupId, oldestId, createdAt, hash FROM " << tableCloneGroups << " JOIN " << tableFilesExtra << " ON oldestId = fileId JOIN " << tableCloneGroupHashes << " ON groupId = id"), [t, & count] (unsigned cols, char ** row) {
        if (++count % 1000000 == 0)
            Thread::Print(STR("      " << count << std::endl), false);
        Merger::AddCloneGroup(t, Hash::Parse(row[3]), CloneGroup(std::atoi(row[0]), std::atoi(row[1]), std::atoi(row[2])));
    });
    Thread::Print(STR("      total: " << Merger::NumCloneGroups(t) << std::endl));

    Thread::Print(STR("    tokens..." << std::endl));
    count = 0;
    sql.query(STR("SELECT first.id, uses, hash FROM " << tableTokens << " AS first JOIN " << tableTokenHashes << " AS second ON first.id = second.id"), [t, & count] (unsigned cols, char ** row) {
        if (++count % 1000000 == 0)
            Thread::Print(STR("      " << count << std::endl), false);
        Merger::AddTokenInfo(t, Hash::Parse(row[2]), TokenInfo(std::atoi(row[0]), std::atoi(row[1])));
    });
    Thread::Print(STR("      total: " << Merger::NumTokens(t) << std::endl));
   #endif
}


template<typename T>
void initializeThreads(unsigned num) {
    Thread::InitializeWorkers<T>(num);
    Thread::Print(STR("    " <<  T::Name() << " (" << num << ")" << std::endl));
}

void stampAndSummary(std::chrono::high_resolution_clock::time_point const & since) {
    auto now = std::chrono::high_resolution_clock::now();
    unsigned secondsSince = std::chrono::duration_cast<std::chrono::seconds>(now - since).count();
    Buffer & stamp = Buffer::Get(Buffer::Kind::Stamp);
    Buffer & summary = Buffer::Get(Buffer::Kind::Summary);

    stamp.append(STR("'last-stride-index'," << ClonedProject::StrideIndex()));

    summary.append(STR("'stride-index'," << ClonedProject::StrideIndex()));
    summary.append(STR("'stride-count'," << ClonedProject::StrideCount()));
    summary.append(STR("'time'," << secondsSince));
    summary.append(STR("'projects'," << Downloader::JobsDone()));
    summary.append(STR("'projects-dropped'," << Downloader::Errors()));
    summary.append(STR("'files'," << Tokenizer::TotalFiles()));
    summary.append(STR("'tokenizer-errors'," << Tokenizer::Errors()));
    summary.append(STR("'generic-files-unique'," << Merger::UniqueFileHashes(TokenizerKind::Generic)));
    summary.append(STR("'generic-files-tokens-unique'," << Merger::UniqueTokenHashes(TokenizerKind::Generic)));
    summary.append(STR("'js-files-unique'," << Merger::UniqueFileHashes(TokenizerKind::JavaScript)));
    summary.append(STR("'js-files-tokens-unique'," << Merger::UniqueTokenHashes(TokenizerKind::JavaScript)));
    //summary.append(STR("," << ));
    Buffer::FlushAll();
}

void run() {
    Thread::Print(STR("Running strided tokenizer, stride N = " << ClonedProject::StrideCount() << ", index = " << ClonedProject::StrideIndex() << std::endl));
    auto start = std::chrono::high_resolution_clock::now();

    DBWriter::CheckDatabase();

    Thread::Print(STR("  initializing threads" << std::endl));
    initializeThreads<CSVReader>(1);
    initializeThreads<Downloader>(50);
    initializeThreads<Tokenizer>(4);
    initializeThreads<Merger>(8);
    initializeThreads<DBWriter>(2);
    initializeThreads<Writer>(1);
    Thread::Print(STR("  scheduling csv file " << "TODO WHICH ONE" << std::endl));
    CSVReader::Schedule("/data/sourcerer/ghtorrent/mysql-2016-11-01/projects.csv");

    // process all projects within current stride
    Thread::Print(STR("  processing..." << std::endl));
    std::string statsOutput;
    while (not stats(statsOutput, start)) {
        Thread::Print(STR(CSI << "[J" << statsOutput << CSI << "[42A"), false);
    }

    // flush database buffers and store state to the database
    Thread::Print(STR("  flushing db buffers and storing the state..." << std::endl));

    std::thread t([]() {
        try {
            Merger::FlushStatistics();
            Buffer::FlushAll();
        } catch (std::string const & e) {
            Thread::Error(e);
        } catch (...) {
            Thread::Error("Unknown error");
        }
    });
    t.detach();

    while (not stats(statsOutput, start)) {
        Thread::Print(STR(CSI << "[J" << statsOutput << CSI << "[42A"), false);
    }
    Thread::Print(STR("  writing stamp..." << std::endl));
    stampAndSummary(start);

    while (not stats(statsOutput, start)) {
        Thread::Print(STR(CSI << "[J" << statsOutput << CSI << "[42A"), false);
    }
    // all is done
    Thread::Print(statsOutput); // print last stats into the logfile as well
    Thread::Print(STR("ALL DONE" << std::endl));
    // todo print last info table
}

void setup(int argc, char * argv[]) {
    // set default targets for the various buffers
    Buffer::TargetType(Buffer::Kind::Stamp) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::Summary) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::Projects) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::ProjectsExtra) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::Files) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::FilesExtra) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::Stats) = Buffer::Target::DB;
    Buffer::TargetType(Buffer::Kind::ClonePairs) = Buffer::Target::File;
    Buffer::TargetType(Buffer::Kind::CloneGroups) = Buffer::Target::File;
    Buffer::TargetType(Buffer::Kind::Tokens) = Buffer::Target::File;
    Buffer::TargetType(Buffer::Kind::TokensText) = Buffer::Target::File;
    Buffer::TargetType(Buffer::Kind::TokenizedFiles) = Buffer::Target::File;

    DBWriter::SetQueueMaxLength(500);

    Writer::SetQueueMaxLength(500);
    TokenizedFile::SetMaxInstances(100000);
    ClonedProject::SetMaxInstances(1000);

    // This has to be done *before* we add tokenizers !!!!!
    ClonedProject::StrideCount() = 100;
    ClonedProject::StrideIndex() = std::atoi(argv[1]);
    DBWriter::ResetDatabase() = ClonedProject::StrideIndex() == 0;

    // set the stride id for the files so that they are unique across strides
    TokenizedFile::InitializeStrideId();


    addTokenizer(TokenizerKind::Generic);
    addTokenizer(TokenizerKind::JavaScript);


    SQLConnection::SetConnection("127.0.0.1", "peta", "pycus");

    // !!!!!!!!
    DBWriter::DatabaseName() = "github_js";
    Downloader::DownloadDir() = "/data/sourcerer/github/download_all";
    Writer::OutputDir() = "/data/sourcerer/github/output_text";
    ClonedProject::KeepProjects() = false;

    CSVReader::SetLanguage("JavaScript");

    Thread::InitializeLog(STR(Writer::OutputDir() << "/log-" << ClonedProject::StrideIndex() << ".txt"));
}



int main(int argc, char * argv[]) {

    //tokenize("/data/sourcerer/github/download/280/Gruntfile.js");
    //tokenize("~/haha.js");
    //return EXIT_SUCCESS;


    try {
        setup(argc, argv);
        run();
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
