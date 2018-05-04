#pragma once

#include <string>
#include <fstream>
#include <functional>

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

    /** Parses single row in CSV format and returns it as a vector of strings.

      Note that a row may span multiple lines.
     */
    static void ParseRow(std::istream & from, std::vector<std::string> & into) {
        std::string line;
        if (not std::getline(from, line))
            line = "";
        unsigned pos = 0;
        while (pos < line.size()) {
            std::string col = "";
            if (line[pos] == QUOTE) {
                ++pos; // for the quote
                while (pos < line.size() and line[pos] != QUOTE) {
                    if (line[pos] == ESCAPE) {
                        ++pos;
                        if (pos >= line.size()) {
                            col += "\n";
                            if (not std::getline(from, line))
                                line = "";
                            pos = 0;
                            continue;
                        } else switch (line[pos]) {
                            case 'b':
                                col += '\b';
                                ++pos;
                                continue;
                            case 'n':
                                col += '\n';
                                ++pos;
                                continue;
                            case 'r':
                                col += '\r';
                                ++pos;
                                continue;
                            case 't':
                                col += '\t';
                                ++pos;
                                continue;
                            case 'z':
                                col += (char)26;
                                ++pos;
                                continue;
                            default:
                                break;
                        }
                    }
                    col += line[pos++];
                }
                if (pos < line.size()) {
                    assert (line[pos] == QUOTE);
                    ++pos; // pop the quote
                }
            } else {
                while (pos < line.size() and line[pos] != DELIMITER)
                    col += line[pos++];
            }
            if (pos < line.size()) {
                assert(line[pos] == DELIMITER);
                ++pos; // pop the delmiter
            }
            into.push_back(col);
        }
    }

    static void ParseFile(std::string const & filename, std::function<void(std::vector<std::string> &)> f) {
        std::ifstream s(filename);
        if (not s.good())
            throw STR("Unable to open CSV file " << filename);
        std::vector<std::string> row;
        while (not s.eof()) {
            row.clear();
            ParseRow(s, row);
            // callback for non-empty rows
            if (not row.empty())
                f(row);
        }
    }

private:



    std::string const & projectLanguage(std::vector<std::string> const & row) {
        return row[5];
    }

    bool isDeleted(std::vector<std::string> const & row) {
        return row[9] == "0";

    }

    bool isForked(std::vector<std::string> const & row) {
        return row[7] != "\\N";

    }

    virtual void process() {
        unsigned counter = 0;
        ParseFile(job_, [&](std::vector<std::string> & row) {
            if (counter++ % ClonedProject::StrideCount() != ClonedProject::StrideIndex())
                return;
            ++totalProjects_;
            if (projectLanguage(row) == language_) {
                ++languageProjects_;
                if (not isDeleted(row)) {
                    if (not isForked(row)) {
                        ++validProjects_;
                        //if (validProjects_ > 10)
                        //    break;
                        // pass the project to the downloader
                        try {
                            Downloader::Schedule(DownloaderJob(new ClonedProject(
                                std::atoi(row[0].c_str()),
                                row[1].substr(29),
                                timestampFrom(row[6]))));
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
        });
/*

        f_.open(job_);
        if (not f_.good())
            throw STR("Unable to open CSV file " << job_);
        while (not f_.eof()) {
            // parse row
            row_ = ParseRow(f_);
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
        f_.close(); */
    }


    std::ifstream f_;
    //std::vector<std::string> row_;

    static std::string language_;

    static std::atomic_uint totalProjects_;
    static std::atomic_uint languageProjects_;
    static std::atomic_uint forkedProjects_;
    static std::atomic_uint deletedProjects_;
    static std::atomic_uint validProjects_;
};
