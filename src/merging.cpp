#include "worker.h"
#include "buffers.h"
#include "workers/CSVReader.h"
#include "workers/Writer.h"
#include "workers/DBWriter.h"

#ifdef HAHA

/** Merges results from two previous runs into a single format.

  Previous runs may be either directly from the tokenizer, or they can be previous results of merger phase as the output format of the merge phase is identical to that of the tokenizer.

  Token information (unified id's and use counts) is merged first, using the following algorithm:

  - all token texts from first are read, immediately outputed and their hashes & ids are remembered
  - all token texts from second are read and translation vector is constructed
    - for those already seen in first, their new id is the id in first
    - for the new ones, they are assigned a new id and thy are outputed

  After this step, we can discard the hashes of the tokens and only keep the translation map and we must update the token uses.

  - create a vector of uses & ids for tokens and read all token uses from *second*. Use tre translation vector to determine where to put the uses information
  - load all tokens from first and update the uses vector (no translation required)
  - output the result uses and discard the datastructure

  Finally, we must output the merged tokenized files and translate tokens from the second:

  - load all tokenized files from first, create a map from their tokenHash to fileId and output them immediately
  -



  Merging Clone Information

  We already have token hashes mapped to ids from first and we have a list of files ids from second which have a same hash file present in first.

  Let's first merge clone pairs. All clone pairs from first can be directly outputed. For clone pairs from second do the following:
  - if the groupId has is unique to second, output them as they are
  - if the groupId exists in first, then do not output the identity (groupId->groupId), and change all groupId's to the groupId from the first. Store the groupId first, groupId second for clone group translation (we will have to determine oldest id for the group)







 */
class ResultsMerger {
public:
    ResultsMerger(int first, int second, TokenizerKind tokenizer):
        first_(first),
        second_(second),
        output_(-second),
        tokenizer_(tokenizer) {
    }

    void merge() {
        // TODO re-enable this back
        //if (first_ == 0)
        //    compactTokenIds(0);

        Thread::Print(STR("Merging " << first_ << " and " << second_ << " into " << output_ << std::endl));
        mergeTokenTexts();
        mergeTokenUses();
        mergeTokenizedFiles();
        Thread::Print(STR("  deleting " << tokensTranslation_.size() << " token translations" << std::endl));
        tokensTranslation_.clear();
        mergeClonePairs();
        mergeCloneGroups();


    }


private:







    void mergeTokenTexts() {
    }

    void mergeTokenUses() {
    }

    /** Merges tokenized files so that only one record per tokens hash will be reported.

      NOTE If we ever experience memory problems this can be rewritten so that we first output everything in the second and then merge in files from the first. This would work just fine and will require only as much memory as a single stride, but when clone groups are also calculated, larger number of files might be affected by the changes and more database searches.
     */
    void mergeTokenizedFiles() {
    }

    /** Merges clone pairs of the two strides.
     */
    void mergeClonePairs() {
    }

    int oldestFrom(SQLConnection & sql, int first, int second) {
    }

    void mergeCloneGroups() {
    }

    TokenizerKind tokenizer_;

    int first_;
    int second_;
    int output_;






    /** List of clone groups defined in second that have no files in first. These clone groups require no updates.
     */
    std::unordered_set<int> secondUniqueCloneGroups_;

    /** All groups from first, which have groups in second as well. For these, the first group will be used, but the oldestId must be checked.
     */
    std::unordered_map<int, int> firstGroupsInSecond_;

    /** Group id in second -> file id in first that is part of it. Their cdates should be merged.
     */
    std::unordered_map<int, int> firstFileInSecond_;

    /** Group id in first -> file id in second. Their cdates should be merged.
     */
    std::unordered_map<int, int> secondFileInFirst_;

    /** first group -> unique file in second that should be part of it
     */
    std::unordered_map<int, int> firstGroupSecondFile;


    int firstTokens_;

};

#endif


/** Merges the results.

  Takes two previous outputs (either directly from strides or from a previous merging and merges the results into a new set of files.

  Token statistics and outputs are merged first:
    - all token texts from first are read, their hash & id remembered and others dumped to output file
    - all token texts from second are read, and their new id is constructed. This is either existing id if they were found in first as well, or a new id (and they are appended to the output)
    - all token statistics from second are read in memory
    - all token statistics from first are read and if there were same tokens in second, their uses are added and results are reported. All tokens from second that were not in first are reported with their counts
    - all tokenized files from the first are read, their hashes remembered and they are reported
    - all tokenized files from the second are read, if their hash is not in first, their tokens are translated and they are reported

  Then the clone pairs and clone groups have to be merged:

  Clone pairs are reported based on their


 */



void mergeResults() {
    //ResultsMerger m(0, 1, TokenizerKind::Generic);
    //m.merge();
}
