#pragma once

/** Number of cpu cores.

 */
#define NUM_CORES 16

/** Default directory names for the output files.

  This is taken from sourcererCC's tokenizer config names, where appropriate. We also introduce couple of our names. See their usage for better description.
 */

#define PATH_STATS_FILE "files_stats"
#define PATH_BOOKKEEPING_PROJS "bookkeeping_projs"
#define PATH_TOKENS_FILE "files_tokens"

#define PATH_CLONES_FILE "clones"
#define PATH_FULL_STATS_FILE "files_full_stats"

#define PATH_DIFFS "diffs"

/** File names for the respective output files.

  This is chosen to correspond with sourcererCC's names, where appropriate.
 */

#define STATS_FILE "files-stats-"
#define STATS_FILE_EXT ".txt"

#define BOOKKEEPING_PROJS "bookkeeping-proj-"
#define BOOKKEEPING_PROJS_EXT ".txt"

#define TOKENS_FILE "files-tokens-"
#define TOKENS_FILE_EXT ".txt"

#define CLONES_FILE "clones-"
#define CLONES_FILE_EXT ".txt"

#define FULL_STATS_FILE "stats-full-"
#define FULL_STATS_FILE_EXT ".txt"
