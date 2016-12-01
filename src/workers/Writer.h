#pragma once

#include <fstream>
#include <set>

#include "../data.h"
#include "../worker.h"

class WriterJob : public std::shared_ptr<TokensMap> {
public:
    WriterJob() {
    }

    WriterJob(std::shared_ptr<TokensMap> ptr):
        std::shared_ptr<TokensMap>(ptr) {
    }

    friend std::ostream & operator << (std::ostream & s, WriterJob const & j) {
        s << "File id " << j->file->id << ", url: " << j->file->project->url << "/" << j->file->relPath;
        return s;
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
        // make sure the output directory exists
        createDirectory(outputDir_);

        // initialize the contexts (i.e. create the output files)
        for (TokenizerKind k : tokenizers_) {
            unsigned idx = static_cast<unsigned>(k);
            if (contexts_.size() < idx + 1)
                contexts_.resize(idx + 1);
            std::string filename = STR(outputDir_ << "/" << prefix(k) << "tokens-" << index << "-" << ClonedProject::StrideIndex() << ".txt");
            contexts_[idx].f.open(filename);
            if (not contexts_[idx].f.good())
                throw STR("Unable to open output file " << filename);
        }
    }

    static std::string & OutputDir() {
        return outputDir_;
    }

    static void AddTokenizer(TokenizerKind k) {
        tokenizers_.insert(k);
    }

private:
    struct Context {
        std::ofstream f;
    };

    void process(TokensMap const & tokens, std::ofstream & f) {
        // do nothing if the file is empty, then sourcerer is not interested
        if (tokens.tokens.size() == 0)
            return;
        // otherwise output what we can
        f << tokens.file->project->id << "," <<
             tokens.file->id << "," <<
             tokens.file->totalTokens << "," <<
             tokens.file->uniqueTokens << "," <<
             escape(tokens.file->tokensHash) << "," <<
             "@#@";
        auto i = tokens.tokens.begin(), e = tokens.tokens.end();
        f << i->first << "@@::@@" << i->second;
        while (++i != e)
            f << "," << i->first << "@@::@@" << i->second;
        f << std::endl;
    }


    void process() override {
        process(*job_, contexts_[static_cast<unsigned>(job_->file->tokenizer)].f);
    }

    static std::set<TokenizerKind> tokenizers_;
    static std::string outputDir_;

    std::vector<Context> contexts_;
};
