#pragma once

#include <fstream>
#include <atomic>

#include "worker.h"

/** Responsible for parsing the input CSV file and creating projects out of it.

  Reads the given CSV file row by row and for each row that corresponds to a non-deleted and non-forked file creates a GitHubProject and passes it to the Downloader threads.
 */
class CSVParser : public QueueWorker<std::string> {
public:
    CSVParser(unsigned index):
        QueueWorker<std::string>("CSV_PARSER", index) {
    }

    static void SetLanguage(std::string const & value) {
        language_ = value;
    }

    static unsigned TotalProjects() {
        return totalProjects_;
    }

    static unsigned LanguageProjects() {
        return languageProjects_;
    }

    static unsigned DeletedProjects() {
        return deletedProjects_;
    }

    static unsigned ForkedProjects() {
        return forkedProjects_;
    }

    static unsigned ValidProjects() {
        return validProjects_;
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


    void getLine();

    void parseRow();

    void process(std::string const & job) override;

    std::ifstream f_;
    std::string line_;
    unsigned pos_;
    std::vector<std::string> row_;




    /** Language to be filtered. */
    static std::string language_;

    /** Total projects seen by all CSV parsers. */
    static std::atomic_uint totalProjects_;
    /** Number of projects in given language seen by the CSV parsers. */
    static std::atomic_uint languageProjects_;
    /** Number of deleted projects in the specified language. */
    static std::atomic_uint deletedProjects_;
    /** Number of existing forked projects in the given language. */
    static std::atomic_uint forkedProjects_;
    /** Number of valid projects in specified language, i.e. those passed down to the downloaders. */
    static std::atomic_uint validProjects_;
};
