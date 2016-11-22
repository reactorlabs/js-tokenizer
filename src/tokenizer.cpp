#include <dirent.h>
#include <cstring>
#include <thread>
#include <fstream>

#include "tokenizer.h"
#include "merger.h"

#include "db.h"

#include "hashes/md5.h"


#include "tokenizers/js.h"



#include "tokenizers/generic.h"


std::atomic_uint Tokenizer::fid_(0);

Tokenizer::Tokenizers Tokenizer::tokenizers_ = Tokenizer::Tokenizers::GenericAndJs;


void Tokenizer::process(TokenizerJob const & job) {
    ClonedProject * p = job.project;
    // first, check if the directory already contains the file with last dates
    std::ifstream cdates(STR(p->path() << "/cdate.js.tokenizer.txt"));
    // if the file does not exist, create it first by running git
    if (not cdates.good()) {
        exec(STR("git log --format=\"format:%at\" --name-only --diff-filter=A > cdate.js.tokenizer.txt"), p->path());
        // reopen the file
        cdates.open(STR(p->path() << "/cdate.js.tokenizer.txt"));
        assert(cdates.good() and "It should really exist now");
    }
    // now that we know the project is valid, add the project to the database
    // TODO add project here
    // Db::addProject(p);
    // let's parse the output now
    int date = 0;
    while (not cdates.eof()) {
        std::string tmp;
        std::getline(cdates, tmp, '\n');
        if (tmp == "") { // if the line is empty, next line will be timestamp
            date = 0;
        } else if (date == 0) { // read timestamp
            date = std::atoi(tmp.c_str());
        } else { // all other lines are actual files
            // TODO this should support multiple languages too
            if (isLanguageFile(tmp))
                tokenize(job.project, tmp, date);
        }
    }
    // project bookkeeping, so that floating projects are deleted when all their files are written and they are no longer needed
    p->release();
}

void Tokenizer::tokenize(ClonedProject * project, std::string const & relPath, int cdate) {
    TokenizedFile * tf = new TokenizedFile(project, ++fid_, relPath);
    if (isFile(tf->path())) {
        Worker::Log(STR("tokenizing " << tf->path()));

        // open the file and load it into the data string
        std::ifstream s(tf->path(), std::ios::in | std::ios::binary);
        if (not s.good())
            throw STR("Unable to open file " << tf->path());
        s.seekg(0, std::ios::end);
        std::string data;
        data.resize(s.tellg());
        s.seekg(0, std::ios::beg);
        s.read(& data[0], data.size());
        s.close();


        // check the file data appears to be valid
        if (data.size() >= 4 and data[0] == 'P' and data[1] =='K' and data[2] == '\003' and data[3] == '\004')
            throw STR("File " << tf->path() << " seems to be archive");

        // set total number of bytes and file hash
        tf->bytes = data.size();

        tf->fileHash = data;

        // since we may have more than one tokenizer, we might need a vector
        std::vector<TokenizedFile *> results;

        switch (tokenizers_) {
            case Tokenizers::Generic:
                results.push_back(tf);
                GenericTokenizer::Tokenize(tf, std::move(data));
                break;
            case Tokenizers::GenericAndJs: {
                TokenizedFile *tf2 = new TokenizedFile(*tf);
                std::string data2 = data;
                GenericTokenizer::Tokenize(tf2, std::move(data2));
                results.push_back(tf2);
                // fallthrough to JS only which adds the JS
            }
            case Tokenizers::JavaScript:
                results.push_back(tf);
                JSTokenizer::Tokenize(tf, std::move(data));
                break;
        }

        // mark as processed so that we do not show weird counters

        processedBytes_ += tf->bytes;
        ++processedFiles_;

        // for any tokenized file, calculate its tokens hash and submit to the merger

        for (TokenizedFile * f : results) {
            MD5 md5;
            for (auto i : f->tokens) {
                md5.add(i.first.c_str(), i.first.size());
                md5.add(& i.second, sizeof(i.second));
            }
            f->tokensHash = md5;

            // attach to the project for this particular file and submit to merger
            f->project->attach();
            switch (f->tokenizer) {
                case TokenizerType::Generic:
                    Merger<TokenizerType::Generic>::Schedule(f, this);
                    break;
                case TokenizerType::JavaScript:
                    Merger<TokenizerType::JavaScript>::Schedule(f, this);
                    break;
            }
        }
    } else {
        // TODO some better error here
        delete tf;
    }
}
