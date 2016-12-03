#pragma once

#include <fstream>
#include <set>

#include "../data.h"
#include "../worker.h"


class WriterJob {
public:

    WriterJob():
        kind(Buffer::Kind::Error),
        tokenizer(TokenizerKind::Generic) {
    }

    WriterJob(Buffer::Kind kind, TokenizerKind tokenizer, std::string && what):
        kind(kind),
        tokenizer(tokenizer),
        what(std::move(what)) {
    }

    Buffer::Kind kind;
    TokenizerKind tokenizer;
    std::string what;

    friend std::ostream & operator << (std::ostream & s, WriterJob const & job) {
        s << job.kind << ": " << job.what.substr(0, 100) << "...";
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
        assert(index == 0 and "Multiple writers currently not supported");
        // initialize the contexts (i.e. create the output files)
        for (TokenizerKind k : tokenizers_) {
            unsigned idx = static_cast<unsigned>(k);
            if (contexts_.size() < idx + 1)
                contexts_.resize(idx + 1);
            std::string outputDir = STR(outputDir_ << "/" << prefix(k));
            createDirectory(outputDir);
            contexts_[idx].initialize(outputDir);
        }
    }

    static std::string & OutputDir() {
        return outputDir_;
    }

    static void AddTokenizer(TokenizerKind k) {
        tokenizers_.insert(k);
    }


private:
    static void openFile(std::ofstream & s, std::string const filename) {
        s.open(filename);
        if (not s.good()) {
            std::cout << " Houston, we have a problem " << filename << std::endl;
            throw STR("Unable to open " << filename);
        }
    }

    struct Context {
        std::ofstream tokenizedFile;
        std::ofstream newToken;
        std::ofstream tokenInfo;
        std::ofstream clonePair;
        std::ofstream cloneGroup;

        void initialize(std::string const & outputDir) {
            Writer::openFile(tokenizedFile, STR(outputDir << "/tokens-" << ClonedProject::StrideIndex() << ".txt"));
            Writer::openFile(newToken, STR(outputDir << "/tokensText-" << ClonedProject::StrideIndex() << ".txt"));
            Writer::openFile(tokenInfo, STR(outputDir << "/tokeninfo-" << ClonedProject::StrideIndex() << ".txt"));
            Writer::openFile(clonePair, STR(outputDir << "/clonepairs-" << ClonedProject::StrideIndex() << ".txt"));
            Writer::openFile(cloneGroup, STR(outputDir << "/clonegroups-" << ClonedProject::StrideIndex() << ".txt"));
        }
    };

    void process(Context & c) {
        switch (job_.kind) {
            case Buffer::Kind::TokenizedFile:
                c.tokenizedFile << job_.what;
                break;
            case Buffer::Kind::TokensText:
                c.newToken << job_.what;
                break;
            case Buffer::Kind::Tokens:
                c.tokenInfo << job_.what;
                break;
            case Buffer::Kind::ClonePairs:
                c.clonePair << job_.what;
                break;
            case Buffer::Kind::CloneGroups:
                c.cloneGroup << job_.what;
                break;
            default:
                throw STR("Buffer kind not supported for File target");
        }
/*


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
        f << std::endl; */
    }


    void process() override {
        process(contexts_[static_cast<unsigned>(job_.tokenizer)]);
    }

    static std::set<TokenizerKind> tokenizers_;
    static std::string outputDir_;

    std::vector<Context> contexts_;
};
