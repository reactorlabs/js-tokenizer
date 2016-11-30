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

// convert assertions to errors

// make errors print current job

// missing token total counts

// JS file stats way too small

// create DB aggregator and DB writer for better load ballancing

// there are three fatal errors in the tokenizer, which is not good

// TODO JS and generic produce diffenet and possibly both wrong line counts

// TODO output even partial results every N projects (this also imposes a barrier)

// TODO produce a database stamp at the end

int lastStride = -1;



void addTokenizer(TokenizerKind k) {
    Tokenizer::AddTokenizer(k);
    Merger::AddTokenizer(k);
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

    unsigned d = seconds / (3600 * 24);
    seconds = seconds % (3600 * 24);

    unsigned h = seconds / 3600;
    seconds = seconds % 3600;
    unsigned m = seconds / 60;
    unsigned s = seconds % 60;
    return STR(d << ":" << std::setfill('0') << std::setw(2) << h << ":" << std::setfill('0') << std::setw(2) << m << ":" << std::setfill('0') << std::setw(2) << s);
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
    memory += (1024 * 1024) * s[4].queueSize; // these are large buffers
    memory += sizeof(WriterJob) * s[5].queueSize;

    // add downloader errors to the # of projects
    double secondsTotal = secondsSince * EXPECTED_PROJECTS / (s[1].errors + projects);


    Thread::LockOutput();
    std::cout << "Statistics                                                     #   %" << std::endl;
    std::cout << "--------------------------------------------------- ------------ ---" << std::endl;
    std::cout << "  Elapsed time                                     " << std::setw(14) << time(secondsSince) << std::setw(4) << pct(secondsSince, secondsTotal) << std::endl;
    std::cout << "    Estimated remaining time                       " << std::setw(14) << time(secondsTotal - secondsSince) << std::endl;
    std::cout << "  Projects                                         " << std::setw(14) << projects << std::setw(4) << pct(Tokenizer::JobsDone(), EXPECTED_PROJECTS) << std::endl;
    std::cout << "  Files                                            " << std::setw(14) << files << std::endl;
    std::cout << "    JS Errors                                      " << std::setw(14) << JavaScriptTokenizer::ErrorFiles() << std::setw(4) << pct(JavaScriptTokenizer::ErrorFiles(), files) << std::endl;
    std::cout << std::endl;




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
    std::cout << "  Unique file hashes                                " << std::setw(12) << Merger::UniqueFileHashes() << std::endl;
    std::cout << "  Generic" << std::endl;
    std::cout << "    Clone groups objects                            " << std::setw(12) << Merger::NumCloneGroups(TokenizerKind::Generic) << std::endl;
    std::cout << "    Token info objects                              " << std::setw(12) << Merger::NumTokens(TokenizerKind::Generic) << std::endl;
    std::cout << "  JavaScript" << std::endl;
    std::cout << "    Clone groups objects                            " << std::setw(12) << Merger::NumCloneGroups(TokenizerKind::JavaScript) << std::endl;
    std::cout << "    Token info objects                              " << std::setw(12) << Merger::NumTokens(TokenizerKind::JavaScript) << std::endl;
    std::cout << std::endl;
    std::cout << "  ASSUMED MEMORY USE (without overhead)             " << std::setw(12) << xbytes(memory) << std::setw(4) << pct(memory / 1024, 64 * 1024 * 1024) << std::endl;
    std::cout << std::endl;

    Thread::UnlockOutput();

    for (Thread::Stats & stats : s)
        if (stats.queueSize > 0 or stats.idle != stats.started)
            return false;
    return true;
}


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
    t2.tokenize();
    std::cout << "generic: " << g->lines << " -- " << g->loc << " -- " << g->sloc << std::endl;
    std::cout << "js:      " << js->lines << " -- " << js->loc << " -- " << js->sloc << std::endl;
}


void checkTables(SQLConnection & sql, TokenizerKind t) {
    std::string p = prefix(t);
    std::cout << "    " << p << DBWriter::TableFiles << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableFiles << " ("
        "fileId BIGINT(6) UNSIGNED NOT NULL,"
        "projectId INT(6) UNSIGNED NOT NULL,"
        "relativeUrl VARCHAR(4000) NOT NULL,"
        "fileHash CHAR(32) NOT NULL,"
        "PRIMARY KEY (fileId),"
        "UNIQUE INDEX (fileId),"
        "INDEX (projectId),"
        "INDEX (fileHash))"));
    // extra information about files
    std::cout << "    " << p << DBWriter::TableFilesExtra << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableFilesExtra << " ("
        "fileId INT NOT NULL,"
        "createdAt INT UNSIGNED NOT NULL,"
        "PRIMARY KEY (fileId),"
        "UNIQUE INDEX (fileId))"));
    // statistics for unique files (based on *file* hash)
    std::cout << "    " << p << DBWriter::TableStats << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableStats << " ("
        "fileHash CHAR(32) NOT NULL,"
        "fileBytes INT(6) UNSIGNED NOT NULL,"
        "fileLines INT(6) UNSIGNED NOT NULL,"
        "fileLOC INT(6) UNSIGNED NOT NULL,"
        "fileSLOC INT(6) UNSIGNED NOT NULL,"
        "totalTokens INT(6) UNSIGNED NOT NULL,"
        "uniqueTokens INT(6) UNSIGNED NOT NULL,"
        "tokenHash CHAR(32) NOT NULL,"
        "PRIMARY KEY (fileHash),"
        "UNIQUE INDEX (fileHash),"
        "INDEX (tokenHash))"));
    // tokenizer clone pairs (no counterpart in sourcerer CC)
    std::cout << "    " << p << DBWriter::TableClonePairs << "_" << ClonedProject::StrideIndex()  << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableClonePairs << "_" << ClonedProject::StrideIndex()  << " ("
        "fileId INT NOT NULL,"
        "groupId INT NOT NULL,"
        "PRIMARY KEY(fileId),"
        "UNIQUE INDEX(fileId))"));
    // tokenizer clone groups (no counterpart in sourcerer CC)
    std::cout << "    " << p << DBWriter::TableCloneGroups << "_" << ClonedProject::StrideIndex()  << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableCloneGroups << "_" << ClonedProject::StrideIndex()  << " ("
        "groupId INT NOT NULL,"
        "oldestId INT NOT NULL,"
        "PRIMARY KEY(groupId),"
        "UNIQUE INDEX(groupId))"));
    // tokens and their freqencies (this is dumped once at the end of the run)
    std::cout << "    " << p << DBWriter::TableTokens << "_" << ClonedProject::StrideIndex() << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableTokens << "_" << ClonedProject::StrideIndex()  << " ("
        "id INT NOT NULL,"
        "uses INT NOT NULL,"
        "PRIMARY KEY(id),"
        "UNIQUE INDEX(id))"));
    // unique tokens, new tokens are added each time they are found
    std::cout << "    " << p << DBWriter::TableTokensText << "_" << ClonedProject::StrideIndex()  << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableTokensText << "_" << ClonedProject::StrideIndex()  <<  " ("
        "id INT NOT NULL,"
        "size INT NOT NULL,"
        "text LONGTEXT NOT NULL,"
        "PRIMARY KEY(id),"
        "UNIQUE INDEX(id))"));
    // token hashes as a shortcut to the state snapshot
    std::cout << "    " << p << DBWriter::TableTokenHashes << "_" << ClonedProject::StrideIndex()  << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableTokenHashes << "_" << ClonedProject::StrideIndex()  << " ("
        "id INT NOT NULL,"
        "hash CHAR(32) NOT NULL,"
        "PRIMARY KEY(id),"
        "UNIQUE INDEX(id))"));
    // clone group hashes as a shortcut to the state snapshots
    std::cout << "    " << p << DBWriter::TableCloneGroupHashes << "_" << ClonedProject::StrideIndex()  << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << p << DBWriter::TableCloneGroupHashes << "_" << ClonedProject::StrideIndex()  << " ("
        "id INT NOT NULL,"
        "hash CHAR(32) NOT NULL,"
        "PRIMARY KEY(id),"
        "UNIQUE INDEX(id))"));
}



void dropDatabase() {
    std::cout << "  deleting database " << DBWriter::DatabaseName() << std::endl;
    SQLConnection sql;
    sql.query(STR("DROP DATABASE " << DBWriter::DatabaseName()));
}

/** Checks that the database specified as output exists. Creates the required tables if not.
 */
void checkDatabase() {
    SQLConnection sql;
    std::cout << "  checking database " << DBWriter::DatabaseName() << std::endl;
    sql.query(STR("CREATE DATABASE IF NOT EXISTS " << DBWriter::DatabaseName()));
    sql.query(STR("USE " << DBWriter::DatabaseName()));
    // create tokenizer agnostic tables
    std::cout << "    " << DBWriter::TableStamp << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << DBWriter::TableStamp << " ("
        "name VARCHAR(100),"
        "value VARCHAR(1000),"
        "PRIMARY KEY (name))"));
    // info about projects compatible with sourcererCC
    std::cout << "    " << DBWriter::TableProjects << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << DBWriter::TableProjects << " ("
        "projectId INT(6) NOT NULL,"
        "projectPath VARCHAR(4000) NULL,"
        "projectUrl VARCHAR(4000) NOT NULL,"
        "PRIMARY KEY (projectId),"
        "UNIQUE INDEX (projectId))"));
    // extra information about projects (creation date and commit hash downloaded)
    std::cout << "    " << DBWriter::TableProjectsExtra << std::endl;
    sql.query(STR("CREATE TABLE IF NOT EXISTS " << DBWriter::TableProjectsExtra << " ("
        "projectId INT NOT NULL,"
        "createdAt INT UNSIGNED NOT NULL,"
        "commit CHAR(40),"
        "PRIMARY KEY (projectId),"
        "UNIQUE INDEX (projectId))"));
    // check tables for tokenizers
    checkTables(sql, TokenizerKind::Generic);
    checkTables(sql, TokenizerKind::JavaScript);
}

void resumeState(SQLConnection & sql, TokenizerKind t) {
    std::cout << "  resuming previous state for tokenizer " << " TODO NAME " << std::endl;
    std::string p = prefix(t);

    std::string tableStats = STR(p << DBWriter::TableStats);
    std::string tableFilesExtra = STR(p << DBWriter::TableFilesExtra);
    std::string tableCloneGroups = STR(p << DBWriter::TableCloneGroups << "_" << lastStride);
    std::string tableCloneGroupHashes = STR(p << DBWriter::TableCloneGroupHashes << "_" << lastStride);
    std::string tableTokens = STR(p << DBWriter::TableTokens << "_" << lastStride);
    std::string tableTokenHashes = STR(p << DBWriter::TableTokenHashes << "_" << lastStride);


    // first get unique hashes, these are just simply all hashes in stats so far
    std::cout << "    unique file hashes..." << std::endl;
    unsigned count = 0;
    sql.query(STR("SELECT fileHash FROM " << tableStats), [t, & count] (unsigned cols, char ** row) {
       if (++count % 1000000 == 0)
           std::cout << "      " << count << std::endl;
       Merger::AddUniqueFileHash(t, Hash::Parse(row[0]));
   });
    std::cout <<"      total: " <<  Merger::UniqueFileHashes(t) << std::endl;

    // get clone groups, each clone group has to be joined with file extra information so that we have oldest date *and* file id
    std::cout << "    clone groups..." << std::endl;
    count = 0;
    sql.query(STR("SELECT groupId, oldestId, createdAt, hash FROM " << tableCloneGroups << " JOIN " << tableFilesExtra << " ON oldestId = fileId JOIN " << tableCloneGroupHashes << " ON groupId = id"), [t, & count] (unsigned cols, char ** row) {
        if (++count % 1000000 == 0)
            std::cout << "      " << count << std::endl;
        Merger::AddCloneGroup(t, Hash::Parse(row[3]), CloneGroup(std::atoi(row[0]), std::atoi(row[1]), std::atoi(row[2])));
    });
    std::cout << "      total: " << Merger::NumCloneGroups(t) << std::endl;

    std::cout << "    tokens..." << std::endl;
    count = 0;
    sql.query(STR("SELECT first.id, uses, hash FROM " << tableTokens << " AS first JOIN " << tableTokenHashes << " AS second ON first.id = second.id"), [t, & count] (unsigned cols, char ** row) {
        if (++count % 1000000 == 0)
            std::cout << "      " << count << std::endl;
        Merger::AddTokenInfo(t, Hash::Parse(row[2]), TokenInfo(std::atoi(row[0]), std::atoi(row[1])));
    });
    std::cout << "      total: " << Merger::NumTokens(t) << std::endl;
}

/** Resumes the state of the last stride, if any in the database.
 */
void resumeState() {
    SQLConnection sql;
    sql.query(STR("USE " << DBWriter::DatabaseName()));
    sql.query(STR("SELECT value FROM " << DBWriter::TableStamp << " WHERE name = 'last-stride-index'"), [] (unsigned cols, char ** row) {
        lastStride = std::atoi(row[0]);
    });
    if (lastStride != -1) {
        resumeState(sql, TokenizerKind::Generic);
        resumeState(sql, TokenizerKind::JavaScript);
    } else {
        std::cout << "  no previous state found";
    }
}

template<typename T>
void initializeThreads(unsigned num) {
    Thread::InitializeWorkers<T>(num);
    std::cout << "    " <<  T::Name() << " (" << num << ")" << std::endl;
}

void stamp() {
    SQLConnection sql;
    sql.query(STR("USE " << DBWriter::DatabaseName()));
    sql.query(STR("REPLACE INTO " << DBWriter::TableStamp << " VALUES( 'last-stride-index', " << ClonedProject::StrideIndex() << ")"));
    sql.query(STR("REPLACE INTO " << DBWriter::TableStamp << " VALUES( 'last-stride-count', " << ClonedProject::StrideCount() << ")"));
}

void run() {
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "  initializing threads" << std::endl;
    initializeThreads<CSVReader>(1);
    initializeThreads<Downloader>(50);
    initializeThreads<Tokenizer>(4);
    initializeThreads<Merger>(4);
    initializeThreads<DBWriter>(4);
    initializeThreads<Writer>(1);
    std::cout << "  scheduling csv file " << "TODO WHICH ONE" << std::endl;
    CSVReader::Schedule("/data/sourcerer/ghtorrent/mysql-2016-11-01/projects.small.csv");

    // process all projects within current stride
    std::cout << "  processing..." << std::endl;
    while (not stats(start)) { }

    // flush database buffers and store state to the database
    std::cout << "  flushing db buffers and storing the state..." << std::endl;
    Downloader::FlushBuffers();
    Merger::FlushBuffers();
    Merger::FlushStatistics();
    while (not stats(start)) { }
    std::cout << "  writing stamp..." << std::endl;
    stamp();

    // all is done
    std::cout << "ALL DONE" << std::endl;
    // todo print last info table
}

void setup() {
    DBWriter::SetQueueMaxLength(500);
    TokenizedFile::SetMaxInstances(100000);
    ClonedProject::SetMaxInstances(1000);

    // This has to be done *before* we add tokenizers !!!!!
    ClonedProject::StrideCount() = 1;
    ClonedProject::StrideIndex() = 0;
    // set the stride id for the files so that they are unique across strides
    TokenizedFile::InitializeStrideId();


    addTokenizer(TokenizerKind::Generic);
    addTokenizer(TokenizerKind::JavaScript);


    SQLConnection::SetConnection("127.0.0.1", "sourcerer", "js");



    DBWriter::DatabaseName() = "github_stride";



    Downloader::SetDownloadDir("/data/sourcerer/github/download");
    Writer::SetOutputDir("/data/sourcerer/github/output");
    ClonedProject::KeepProjects() = true;





    CSVReader::SetLanguage("JavaScript");

}


/**
 */
void runStride() {
    std::cout << "Running strided tokenizer, stride N = " << ClonedProject::StrideCount() << ", index = " << ClonedProject::StrideIndex() << std::endl;

    dropDatabase();
    checkDatabase();
    resumeState();
    run();

/*

    sql.query(STR("USE " << DB_NAME));
    unsigned count = 0;
    sql.query("SELECT fileHash FROM stats", [ & count] (unsigned cols, char ** row) {
        ++count;
        std::cout << row[0] << std::endl;
    });
    std::cout << "Number of rows: " << count << std::endl; */
}



int main(int argc, char * argv[]) {
    //tokenize("/data/sourcerer/github/download/202/apps/system/js/accessibility_quicknav_menu.js");
    //return EXIT_SUCCESS;


    try {
        setup();
        runStride();
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;

    // let's make this artificially high so that DB has time to rest, let the # of objects do the job

    //ClonedProject::SetKeepProjects(true);





    CSVReader::Schedule("/data/sourcerer/ghtorrent/mysql-2016-11-01/projects.csv");

    Thread::InitializeWorkers<DBWriter>(4);
    Thread::InitializeWorkers<CSVReader>(1);
    Thread::InitializeWorkers<Downloader>(50);
    Thread::InitializeWorkers<Tokenizer>(4);
    Thread::InitializeWorkers<Merger>(4);
    Thread::InitializeWorkers<Writer>(1);

    auto start = std::chrono::high_resolution_clock::now();
    // wait for the tokenizer to finish
    while (not stats(start)) { }
    // flush merger stats and wait for all the jobs to finish
    Downloader::FlushBuffers();
    Merger::FlushBuffers();
    Merger::FlushStatistics();
    while (not stats(start)) { }
    // finally

    return EXIT_SUCCESS;
}
