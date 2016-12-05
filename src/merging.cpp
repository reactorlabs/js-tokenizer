#include "worker.h"
#include "buffers.h"
#include "workers/CSVReader.h"
#include "workers/Writer.h"
#include "workers/DBWriter.h"



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










 */
class ResultsMerger {
public:



private:

    void mergeTokenTexts() {
        std::string i1;
        std::string i2;
        std::ofstream o;
        std::unordered_map<Hash, int> tokens;

        // load tokens from the first file
        Thread::Print(STR("  loading token hashes from " << i1 << std::endl));
        unsigned counter = 0;
        CSVReader::ParseFile(i1, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            Hash h = Hash::Parse(row[2].c_str());
            // add to tokens
            tokens[h] = id;
            // output directly
            o << row[0] << "," << row[1] << ",\"" << row[2] << "\",\"" << row[3] << "\"" << std::endl;
            // increase how many tokens we have seen
            ++counter;
        });
        Thread::Print(STR("    " << counter << " unique tokens found" << std::endl));

        // now load all tokens from the second one and fill in the translation table
        Thread::Print(STR("  loading & translating token hashes from " << i2 << std::endl));
        unsigned secondTotal = 0;
        unsigned secondUnique = 0;
        CSVReader::ParseFile(i2, [&] (std::vector<std::string> & row) {
            ++secondTotal;
            int id = std::atoi(row[0].c_str());
            Hash h = Hash::Parse(row[2].c_str());
            assert(tokensTranslation_.size() == id);
            // if the hash is already known, get the id and do not output it
            auto &i = tokens.find(h);
            if (i != tokens.end()) {
                int newId = i->second;
                tokensTranslation_.push_back(newId);
            } else {
                ++secondUnique;
                // otherwise obtain new id, and output with the new id
                int newId = counter++;
                tokensTranslation_.push_back(newId);
                o << newId << "," << row[1] << ",\"" << row[2] << "\",\"" << row[3] << "\"" << std::endl;
            }
        });
        Thread::Print(STR("    " << secondUnique << " unique tokens found out of " << secondTotal << std::endl));
        Thread::Print(STR("    " << counter << " unique tokens after the merge" << std::endl));

    }

    std::string first;
    std::string second;
    std::string output;

    std::vector<int> tokensTranslation_;

};



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
    Thread::Print(STR("Merging results..." << std::endl));






}
