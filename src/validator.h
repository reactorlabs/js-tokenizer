#pragma once
#include <thread>

#include "data.h"
#include "worker.h"

#ifdef HAHA


class Validator : public Worker {
public:
    Validator(unsigned index, unsigned start, unsigned stride):
        Worker(STR("VALIDATOR " << index)),
        start_(start),
        stride_(stride) {
    }

    static unsigned Errors() {
        return errors_;
    }

    static unsigned Identical() {
        return identical_;
    }

    static unsigned WhitespaceDifferent() {
        return whitespaceDifferent_;
    }

    static unsigned CommentDifferent() {
        return commentDifferent_;
    }

    static unsigned SeparatorDifferent() {
        return separatorDifferent_;
    }

    static void DisplayStats(double duration);

    static void Initialize(std::string const & outputDir);

    static void InitializeThreads(unsigned num);

    void operator () ();

private:

    bool checkTokensHash(std::string const & first, std::string const & second, bool ignoreWhitespace, bool ignoreComments, bool ignoreSeparators);

    void createDiff(std::string const & subdir, FileStats * first, FileStats * second);


    void analyzeDiff(FileStats * one, FileStats * two);

    unsigned start_;
    unsigned stride_;

    static std::string outputDir_;

    static std::atomic_uint errors_;
    static std::atomic_uint identical_;
    static std::atomic_uint whitespaceDifferent_;
    static std::atomic_uint commentDifferent_;
    static std::atomic_uint separatorDifferent_;

};
#endif
