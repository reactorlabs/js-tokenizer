#pragma once

#include "languages/javascript.h"



#define NUM_THREADS 1

#define PATH_OUTPUT "/home/peta/sourcerer/processedLAST"

#define PATH_INPUT(ENTRY) \
    ENTRY("/home/peta/sourcerer/data")


#define PATH_STATS_FILE "files_stats"
#define PATH_BOOKKEEPING_PROJS "bookkeeping_projs"
#define PATH_TOKENS_FILE "files_tokens"
#define PATH_OUR_DATA_FILE "our_data"

#define PROJECT_ID_HINT 0

#define FILE_ID_HINT 0

#define SOURCERERCC_IGNORE_EMPTY_FILES 1

#define SOURCERERCC_IGNORE_FILE_HASH_EQUIVALENTS 1

#define SOURCERERCC_IGNORE_TOKENS_HASH_EQUIVALENTS 1



