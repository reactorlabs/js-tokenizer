#include "CSVParser.h"
#include "downloader.h"


std::string CSVParser::language_ = "";
std::atomic_uint CSVParser::totalProjects_(0);
std::atomic_uint CSVParser::languageProjects_(0);
std::atomic_uint CSVParser::deletedProjects_(0);
std::atomic_uint CSVParser::forkedProjects_(0);
std::atomic_uint CSVParser::validProjects_(0);



void CSVParser::getLine() {
    if (not std::getline(f_, line_))
        line_ = "";
    pos_ = 0;
}

void CSVParser::parseRow() {
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


void CSVParser::process(std::string const & value) {
    f_.open(value);
    if (not f_.good())
        throw STR("Unable to open CSV file " << value);
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
