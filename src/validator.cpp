#include "validator.h"
#include "escape_codes.h"

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
