#pragma once

#include <string>

namespace sql {
    class Connection;
}


class ClonedProject;
class TokenizedFile;

class Db {
public:
    Db(std::string const & host, std::string const & user, std::string const &password);

    void query(std::string const & query);

    void addProject(ClonedProject * project);

    void addFile(TokenizedFile * file);





private:
    sql::Connection * c_;
};


/**

 fileTokens = token information for non-cloned files

 fileStats = basic statistics about *all* files

 projects = basic statitics about *all* projects that were downloaded (i.e. language non-forks)
 [ used to be bookkeeping projs]


 [ this is extra:]
 clonePairs = clone pairs as reported by the tokenizer (projectId, fileId, projectId, fileId)

 tokens = provides mapping from indices to tokens, indexable to md5

 tokenFreqs = provides information about tokens and their frequencies

 [ and for other tokenizers, these will be prefixed ]

i.e.

 js_fileTokens
 js_clonePairs
 js_tokens
 js_clonePairs









 */