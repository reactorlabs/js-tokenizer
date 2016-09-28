#pragma once
#include <thread>

#include "data.h"



class Validator {
public:
    static std::pair<unsigned, unsigned> validateExactClones() {
        unsigned num = 0;
        unsigned errors = 0;
        for (size_t i = 0, e = CloneInfo::numClones(); i != e; ++i) {
            CloneInfo * ci = CloneInfo::get(i);
            FileStats * first = FileStats::get(ci->fid1());
            FileStats * second = FileStats::get(ci->fid2());
            // todo this is bruteforce validation, we can hash files already seen, etc.
            if (first->fileHash() == second->fileHash()) {
                ++num;
                if (loadEntireFile(first->absPath()) != loadEntireFile(second->absPath()))
                    ++errors;
            }
        }
        return std::pair<unsigned, unsigned>(num, errors);
    }

};
