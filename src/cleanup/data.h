#pragma once

#include <memory>
#include <unordered_map>

#include "helpers.h"

/** Identifies the tokenizer in case when multiple tokenizers are being used.
 */
enum class TokenizerKind {
    Generic,
    JavaScript
};

/** Contains the information about a single cloned project.
 */

class ClonedProject {
public:

    std::string storeIntoTableQuery(std::string const & table) {



    }


    int id;
    std::string url;
    unsigned created_at;
};

/** Contains a tokenized file.
 */
class TokenizedFile {
public:
    TokenizerKind tokenizer;
    int id;
    std::shared_ptr<ClonedProject> project;
    std::string relPath;
    unsigned created_at;

    // Errors observed while tokenizing the file
    unsigned errors;

    unsigned totalTokens;
    unsigned lines;
    unsigned loc;
    unsigned sloc;
    unsigned bytes;
    unsigned whitespaceBytes;
    unsigned commentBytes;
    unsigned separatorBytes;
    unsigned tokenBytes;
    hash fileHash;
    hash tokensHash;

    // Information about the clone group, if the file belongs to one, or -1
    int cloneId;

    // True if the file has unique file hash
    bool fileHashUnique;

    std::unordered_map<std::string, unsigned> tokens;
};
