#pragma once

#include <string>
#include <fstream>

#include "../data.h"
#include "../worker.h"

#include "Downloader.h"

class CSVReader : public Worker<std::string> {
public:
    static char const * Name() {
        return "CSV READER";
    }

    CSVReader(unsigned index):
        Worker<std::string>(Name(), index) {
    }

    static void SetLanguage(std::string const & value) {
        language_ = value;
    }

    static char const DELIMITER = ',';
    static char const QUOTE = '"';
    static char const ESCAPE = '\\';

private:

    bool eol() {
        return pos_ >= line_.size();
    }

    char top() {
        return line_[pos_];
    }

    char pop() {
        char result = line_[pos_];
        if (pos_ < line_.size())
            ++pos_;
        return result;
    }

    void getLine() {
        if (not std::getline(f_, line_))
            line_ = "";
        pos_ = 0;
    }

    void parseRow() {
        row_.clear();
        getLine();
        while (not eol()) {
            // parse single column
            std::string col = "";
            if (top() == QUOTE) {
                pop(); // the quote
                while (not eol() and top() != QUOTE) {
                    // if escaped, skip escape and make sure we add the next character
                    if (top() == ESCAPE) {
                        pop();
                        // if there is end of line after the ESCAPE, it is EOL in some column
                        if (eol()) {
                            getLine();
                            col += '\n';
                            continue;
                        }
                    }
                    col += pop();
                }
                // pop the terminating quote
                if (pop() != QUOTE)
                    throw STR("Unterminated end of column, line " << line_);
            } else {
                while (not eol() and top() != DELIMITER)
                    col += pop();
            }
            // add the column
            row_.push_back(col);
            // if we see delimiter, repeat for next column
            if (top() == DELIMITER) {
                pop();
                continue;
            }
            break;
        }
    }

    std::string const & projectLanguage() {
        return row_[5];
    }

    bool isDeleted() {
        return row_[9] == "0";

    }

    bool isForked() {
        return row_[7] != "\\N";

    }

    virtual void process() {
        unsigned counter = 0;
        f_.open(job_);
        if (not f_.good())
            throw STR("Unable to open CSV file " << job_);
        while (not f_.eof()) {
            // parse row
            parseRow();
            if (row_.size() > 0) {
                // use only every n-th project (this is the stride)
                if (counter++ % ClonedProject::StrideCount() != ClonedProject::StrideIndex())
                    continue;
                ++totalProjects_;
                if (projectLanguage() == language_) {
                    ++languageProjects_;
                    if (not isDeleted()) {
                        if (not isForked()) {
                            ++validProjects_;
                            //if (validProjects_ > 10)
                            //    break;
                            // pass the project to the downloader
                            try {
                                Downloader::Schedule(DownloaderJob(new ClonedProject(
                                    std::atoi(row_[0].c_str()),
                                    row_[1].substr(29),
                                    timestampFrom(row_[6]))));
                            } catch (...) {
                                Error("Invalid projects row");
                                ++errors_;
                            }
                        } else {
                            ++forkedProjects_;
                        }
                    } else {
                        ++deletedProjects_;
                    }
                }
            }
        }
        f_.close();
    }


    std::ifstream f_;
    std::string line_;
    unsigned pos_;
    std::vector<std::string> row_;

    static std::string language_;

    static std::atomic_uint totalProjects_;
    static std::atomic_uint languageProjects_;
    static std::atomic_uint forkedProjects_;
    static std::atomic_uint deletedProjects_;
    static std::atomic_uint validProjects_;
};
