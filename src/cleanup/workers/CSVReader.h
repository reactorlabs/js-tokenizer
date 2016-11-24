#pragma once

#include <fstream>

#include "../worker.h"

class CSVReader : public Worker<std::string> {
public:
    CSVReader(unsigned index):
        Worker<std::string>("CSV READER", index) {
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


    virtual void process() {
        f_.open(job_);
        if (not f_.good())
            throw STR("Unable to open CSV file " << job_);
        while (not f_.eof()) {
            // parse row
            parseRow();
            if (row_.size() > 0) {
                ++totalProjects_;
                if (ClonedProject::language(row_) == language_) {
                    ++languageProjects_;
                    if (not ClonedProject::isDeleted(row_)) {
                        if (not ClonedProject::isForked(row_)) {
                            ++validProjects_;
                            ++processedFiles_;
                            // pass the project to the downloader
                            Downloader::Schedule(new ClonedProject(row_), this);
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
};
