#include <thread>
#include <fstream>

#include "validator.h"
#include "escape_codes.h"
#include "tokenizers/js.h"

std::string Validator::outputDir_;
std::atomic_uint Validator::errors_;
std::atomic_uint Validator::identical_;
std::atomic_uint Validator::whitespaceDifferent_;
std::atomic_uint Validator::commentDifferent_;
std::atomic_uint Validator::separatorDifferent_;

void Validator::DisplayStats(double duration) {
    Worker::LockOutput();
    std::cout << eraseDown;
    std::cout << "Elapsed    " << time(duration) << " [h:mm:ss]" << std::endl << std::endl;

    std::cout << "Active threads " << Worker::NumActiveThreads() << std::endl << std::endl;

    std::cout << "Identical  " << identical_ << std::endl;
    std::cout << "Whitespace " << whitespaceDifferent_ << std::endl;
    std::cout << "Comments   " << commentDifferent_ << std::endl;
    std::cout << "Separator  " << separatorDifferent_ << std::endl;
    std::cout << "Errors     " << errors_ << std::endl;

    unsigned total = identical_ + whitespaceDifferent_ + commentDifferent_ + separatorDifferent_ + errors_;

    std::cout << "Finished   " << total << pct(total, CloneInfo::numClones()) << std::endl;

    std::cout << cursorUp(10);
    Worker::UnlockOutput();
}

void Validator::Initialize(std::string const & outputDir) {
    outputDir_ = outputDir;
    // load projects
    GitProject::parseFile(STR(outputDir << "/" << PATH_BOOKKEEPING_PROJS << "/" << BOOKKEEPING_PROJS << "0" << BOOKKEEPING_PROJS_EXT));
    Worker::Log(STR("loaded " << GitProject::NumProjects() << " projects"));
    // load file stats
    FileStats::parseFile(STR(outputDir << "/" << PATH_FULL_STATS_FILE << "/" << FULL_STATS_FILE << "0" << FULL_STATS_FILE_EXT));
    Worker::Log(STR("loaded " << FileStats::NumFiles() << " file statistics"));
    // load clone info
    CloneInfo::parseFile(STR(outputDir << "/" << PATH_CLONES_FILE << "/" << CLONES_FILE << "0" << CLONES_FILE_EXT));
    Worker::Log(STR("loaded " << CloneInfo::numClones() << " clone pairs"));

    createDirectory(outputDir + "/" + PATH_DIFFS + "/errors");
    createDirectory(outputDir + "/" + PATH_DIFFS + "/separators");
    createDirectory(outputDir + "/" + PATH_DIFFS + "/comments");
    createDirectory(outputDir + "/" + PATH_DIFFS + "/whitespace");
}


void Validator::InitializeThreads(unsigned num) {
    Worker::Log(STR("Initializing " << num << " validator threads"));
    for (unsigned i = 0; i < num; ++i) {
        std::thread t([i, num] () {
            Validator v(i, i, num);
            v();
        });
        t.detach();
    }
}

void Validator::operator () () {
    Worker::activate();
    Worker::Log(STR("Activated from " << start_ << ", stride " << stride_));
    for (size_t i = start_, e = CloneInfo::numClones(); i < e; i += stride_) {
        CloneInfo * ci = CloneInfo::get(i);
        FileStats * first = FileStats::get(ci->fid1());
        FileStats * second = FileStats::get(ci->fid2());
        // if files are file hash equals, make sure they are identical
        if (first->fileHash() == second->fileHash()) {
            if (loadEntireFile(first->absPath()) != loadEntireFile(second->absPath())) {
                Worker::Log(STR("Files " << first->absPath() << " and " << second->absPath() << " are not identical"));
                ++errors_;
            } else {
                ++identical_;
            }
        } else {
            assert(first->tokensHash() == second->tokensHash());
            analyzeDiff(first, second);
        }
    }
    Worker::deactivate();
}

bool Validator::checkTokensHash(std::string const & first, std::string const & second, bool ignoreWhitespace, bool ignoreComments, bool ignoreSeparators) {
    JSTokenizer::IgnoreWhitespace() = ignoreWhitespace;
    JSTokenizer::IgnoreComments() = ignoreComments;
    JSTokenizer::IgnoreSeparators() = ignoreSeparators;
    TokenizedFile tf1;
    TokenizedFile tf2;
    JSTokenizer::Tokenize(tf1, first);
    JSTokenizer::Tokenize(tf2, second);
    if (tf1.stats.errors > 0 or tf2.stats.errors > 0)
        throw STR("Tokenizer reports errors in diffed files");
    tf1.calculateTokensHash();
    tf2.calculateTokensHash();
    return tf1.stats.tokensHash() == tf2.stats.tokensHash();
}

void Validator::createDiff(std::string const & subdir, FileStats * first, FileStats * second) {
    std::string diffName = STR(first->id_ << "_" << second->id_ << ".txt");
    system(STR("diff \"" << first->absPath() << "\" \"" << second->absPath() << "\" >" << outputDir_ << "/" << PATH_DIFFS << "/" << subdir << "/" << diffName).c_str());
}


void Validator::analyzeDiff(FileStats * one, FileStats * two) {
    try {
        std::string first = loadEntireFile(one->absPath());
        std::string second = loadEntireFile(two->absPath());
        if (not checkTokensHash(first, second, true, true, false)) {
            ++separatorDifferent_;
            createDiff("separators", one, two);
        } else if (not checkTokensHash(first, second, true, false, true)) {
            ++commentDifferent_;
            createDiff("comments", one, two);
        } else {
            ++whitespaceDifferent_;
            createDiff("whitespace", one, two);
        }
        return;
    } catch (std::string const & e) {
        Worker::Log(STR(e << " when checking " << one->absPath() << " and " << two->absPath()));
    } catch (...) {
        Worker::Error(STR("Unspecified error when checking " << one->absPath() << " and " << two->absPath()));
    }
    createDiff("errors", one, two);
    ++errors_;
}

