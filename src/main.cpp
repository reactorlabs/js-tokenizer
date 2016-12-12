#ifndef __x86_64__
#error "Must use 64 bit system!"
#endif

#include <cstring>
#include <iomanip>
#include <thread>

#include "workers/CSVReader.h"

std::string CSV_file = "/data/sourcerer/ghtorrent/mysql-2016-11-01/projects.small.csv";


void tokenize();
void mergeResults();

void addTokenizer(TokenizerKind k) {
    Tokenizer::AddTokenizer(k);
    Merger::AddTokenizer(k);
}

void loadDefaults() {
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

    ClonedProject::StrideCount() = 1;
    ClonedProject::StrideIndex() = -1;

    SQLConnection::SetConnection("127.0.0.1", "peta", "pycus");
    CSVReader::SetLanguage("JavaScript");

    DBWriter::DatabaseName() = "github_js";
    Downloader::DownloadDir() = "/data/sourcerer/github/download";
    Writer::OutputDir() = "/data/sourcerer/github/output";
    ClonedProject::KeepProjects() = false;

    addTokenizer(TokenizerKind::Generic);
    addTokenizer(TokenizerKind::JavaScript);
}


void setup(int argc, char * argv[]) {
    if (argc < 3)
        throw STR("Invalid number of arguments.");

    loadDefaults();

    if (strcmp(argv[1], "merge") == 0) {
        DBWriter::DatabaseName() = argv[2];
        Writer::OutputDir() = STR(Writer::OutputDir() << "/" << argv[2]);



    } else {
        if (argc < 4)
            throw STR("Invalid number of arguments.");
        if (argc < 5)
            std::cerr << "CSV file not specified, using default value " << CSV_file << std::endl;
        else
            CSV_file = argv[4];
        // TODO
        std::cout << "!!! IF RUNNING ON GITHUB ALL MAKE SURE STRIDE COUNT IS 100 AND PROPER STRIDE INDICES ARE GIVEN !!!" << std::endl;
        ClonedProject::StrideCount() = std::atoi(argv[1]);
        ClonedProject::StrideIndex() = std::atoi(argv[2]);
        DBWriter::ResetDatabase() = ClonedProject::StrideIndex() == 0;
        // set the stride id for the files so that they are unique across strides
        TokenizedFile::InitializeStrideId();
        // the last argument is database name, which will also be appended to the default output directory
        DBWriter::DatabaseName() = argv[3];
        Writer::OutputDir() = STR(Writer::OutputDir() << "/" << argv[3]);
    }

    // last thing to do is check whether we should pause
    if (isFile("stop.info")) {
        std::cout << "stop.info file found... Waiting for user action to continue..." << std::endl;
        std::string x;
        std::cin >> x;
        std::cout << "resuming execution..." << std::endl;
    }

    Thread::InitializeLog(STR(Writer::OutputDir() << "/log-" << ClonedProject::StrideIndex() << ".txt"));
}










/** Usage:

  tokenizer COUNT INDEX db_name CSV_file

 */
int main(int argc, char * argv[]) {
    try {
        setup(argc, argv);
        if (strcmp(argv[1],"merge") == 0)
            mergeResults();
        else
            tokenize();
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
