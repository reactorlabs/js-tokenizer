#ifndef __x86_64__
#error "Must use 64 bit system!"
#endif

#include <iomanip>
#include <thread>

#include "data.h"
#include "buffers.h"
#include "worker.h"

#include "workers/CSVReader.h"

void tokenize();

void addTokenizer(TokenizerKind k) {
    Tokenizer::AddTokenizer(k);
    Merger::AddTokenizer(k);
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


    // last thing to do is check whether we should pause
    if (isFile("stop.info")) {
        std::cout << "stop.info file found... Waiting for user action to continue..." << std::endl;
        std::string x;
        std::cin >> x;
        std::cout << "resuming execution..." << std::endl;
    }

    Thread::InitializeLog(STR(Writer::OutputDir() << "/log-" << ClonedProject::StrideIndex() << ".txt"));
}












int main(int argc, char * argv[]) {
    try {
        setup(argc, argv);
        tokenize();
    } catch (std::string const & e) {
        std::cerr << e << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
