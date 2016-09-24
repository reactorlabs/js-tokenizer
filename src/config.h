#pragma once

#define JS_TOKENIZER_ENTIRE_FILE 1



/** This include file contains the definitions for the particular language.

 Different languages may be supported by compiling the tokenizer with different language files.
 */
#include "languages/javascript.h"

/** Path to tokenizer's output.

  This path will be created and it is better if it does not exist. In it the PATH_STATS_FILE, PATH_BOOKKEEPING_PROJS, PATH_TOKENS_FILE and PATH_OUR_DAT_FILE folders will be created, into which the respective output files will be put by the writers.
 */
#define PATH_OUTPUT "/home/peta/sourcerer/processedV4"

/** Path to the projects.

  Multiple roots can be specified. Any subdirectory that is a git project will be considered and tokenized.

  The tokenizer determines the git origin url by reading the `.git/config` file, which has to be present.
 */
#define PATH_INPUT(ENTRY) \
    ENTRY("/home/peta/sourcerer/data/jakub2")


/** If 1, each thread will input additional information about what it is doing.
 */
#define VERBOSE 0

/** Crawlers are worker threads responsible for walking the directory structure, finding git projects and tokenizing the files they contain.

  This number defines the number of crawler threads that will be created by the tokenizer.
 */
#define NUM_CRAWLERS 1

/** When crawler thread finds a git project, it tokenizes all its files in the same thread, but if it finds a regular directory, it may operate in two modes:

  - if the crawler's job queue is smaller than this number, the crawler will append any subdirectory it finds to the job queue
  - if the job queue is longer than this number, the crawler will recurse into the directory itself
 */
#define CRAWLER_QUEUE_THRESHOLD (1000 * NUM_CRAWLERS)

/** Writers are workers responsible for writing the stats and tokens records of tokenized files as well as other bookkeeping.

  Each writer thread has its own output files in each of the output directories.

  Usually there more writer threads are of little help as the bottleneck of writer thread is the disk speed.
 */
#define NUM_WRITERS 1


/** These are the names of output directories for different outputs. The names are identical to SourcererCC's default tokenizer.
 */
#define PATH_STATS_FILE "files_stats"
#define PATH_BOOKKEEPING_PROJS "bookkeeping_projs"
#define PATH_TOKENS_FILE "files_tokens"

/** Output to our data format that outputs all files and all statistics we analyze.
 */
#define PATH_OUR_DATA_FILE "our_data"


/** This ignores crawling the `.git` folder in git projects. I think there should be no reason to turn this off.
 */
#define IGNORE_GIT_FOLDER 1

/** When 1, does not output empty files to sourcererCC statistics.
 */
#define SOURCERERCC_IGNORE_EMPTY_FILES 1

/** When 1, does not output entire file hash equivalent files to sourcererCC, outputs only the first one.

  Note that if two files are hash-equivalent, they must be also tokens-hash equivalent.
 */
#define SOURCERERCC_IGNORE_FILE_HASH_EQUIVALENTS 1

/** When 1, does not output tokens hash equivalent files to sourcererCC, outputs only the first one.
 */
#define SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS 1

