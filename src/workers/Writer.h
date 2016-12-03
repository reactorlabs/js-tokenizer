#pragma once

#include <fstream>
#include <set>

#include "../data.h"
#include "../buffers.h"
#include "../worker.h"


class WriterJob {
public:

    WriterJob():
        filename("ERROR") {
    }

    WriterJob(std::string const & filename, std::string && what):
        filename(filename),
        what(std::move(what)) {
    }

    std::string filename;
    std::string what;

    friend std::ostream & operator << (std::ostream & s, WriterJob const & job) {
        s << "File " << job.filename << ": " << job.what.substr(0, 100) << "...";
    }
};

/** Writes the tokens map to the output files based on the tokenizer used.
 */
class Writer : public Worker<WriterJob> {
public:
    static char const * Name() {
        return "FILE WRITER";
    }
    Writer(unsigned index):
        Worker<WriterJob>(Name(), index) {
    }

    static std::string & OutputDir() {
        return outputDir_;
    }

    /** Close all files.
     */
    static void Finalize() {
        for (auto i : outputFiles_)
            delete i.second;
    }

private:
    struct OutputFile {
        std::ofstream file;
        std::mutex m_;
        OutputFile(std::string const & filename) {
            std::string x = STR(Writer::OutputDir() << "/" << filename);
            Thread::Print(STR("  creating file " << x << std::endl));
            createDirectoryForFile(x);
            file.open(x);
            if (not file.good())
                throw STR("Unable to open " << x);
        }
    };

    OutputFile & getOutputFile(std::string const & filename) {
        std::lock_guard<std::mutex> g(m_);
        auto i = outputFiles_.find(filename);
        if (i == outputFiles_.end()) {
            OutputFile * of = new OutputFile(filename);
            outputFiles_[filename] = of;
            return * of;
        }
        return * i->second;
    }

    void process() override {
        OutputFile & of = getOutputFile(job_.filename);
        std::lock_guard<std::mutex> g(of.m_);
        of.file << job_.what;
    }

    static std::string outputDir_;

    static std::mutex m_;

    static std::unordered_map<std::string, OutputFile *> outputFiles_;

};
