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

    static void OpenFile(std::ofstream & f, std::string const & filename) {
        f.open(filename);
        if (not f.good())
            throw STR("Unable to open output file " << filename);
    }

    static int HexChar(char x) {
        return (x >= 'a') ? ( 10 + x - 'a') : x - '0';
    }

    static std::pair<int, int> DecodeTokenMap(std::string const & item) {
        int id = 0;
        unsigned i = 0;
        while (item[i] != '@')
            id = id * 16 + HexChar(item[i++]);
        i += 6; // @@::@@
        int count = std::atoi(item.c_str() + i);
        return std::pair<int, int>(id, count);
    }


    void mv(Buffer::Kind kind) {
        exec(STR("cp " << Buffer::FileName(kind, tokenizer_) << " " << Buffer::FileName(kind, tokenizer_) << ".old"), Writer::OutputDir());
    }

    /** Checks that the given stride's data seem to be valid.
     */
    void checkStride(int index) {

    }

    /** Because token IDs are shared between tokenizers the token id numbers are not consecutive. Because the merger always compacts the second input, we can generalize and let the merger assume that first input is already compacted, which means that in the case of merging 0th and 1st stride we must first compact the first stride results before going to the merger.
      */
    void compactTokenIds(int stride) {
        Thread::Print(STR("Compacting stride "<< stride << "..." << std::endl));
        ClonedProject::StrideIndex() = stride;
        Thread::Print(STR("  renaming original files" << std::endl));
        mv(Buffer::Kind::TokensText);
        mv(Buffer::Kind::Tokens);
        mv(Buffer::Kind::TokenizedFiles);
        std::unordered_map<int, int> translation;
        {
            Thread::Print(STR("  translating tokens texts..." << std::endl));
            std::ofstream o;
            OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokensText, tokenizer_)));
            int counter = 0;
            CSVReader::ParseFile(STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokensText, tokenizer_) << ".old"), [&] (std::vector<std::string> & row) {
                int id = std::atoi(row[0].c_str());
                int newId = counter++;
                translation[id] = newId;
                o << newId  << "," << row[1] << ",\"" << row[2] << "\"," << escape(row[3]) << std::endl;
            });
            Thread::Print(STR("    " << counter << " ids compacted" << std::endl));
        }
        {
            Thread::Print(STR("  translating token uses..." << std::endl));
            std::ofstream o;
            OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::Tokens, tokenizer_)));
            int counter = 0;
            CSVReader::ParseFile(STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::Tokens, tokenizer_) << ".old"), [&] (std::vector<std::string> & row) {
                int id = std::atoi(row[0].c_str());
                assert (translation.find(id) != translation.end() and "tokens_test and tokens info differs");
                int newId = translation[id];
                o << newId  << "," << row[1] << std::endl;
                ++counter;
            });
            Thread::Print(STR("    " << counter << " ids translated" << std::endl));
        }
        {
            Thread::Print(STR("  translating tokenized files..." << std::endl));
            std::ofstream o;
            OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokenizedFiles, tokenizer_)));
            int counter = 0;
            CSVReader::ParseFile(STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokenizedFiles, tokenizer_) << ".old"), [&] (std::vector<std::string> & row) {
                row[5] = row[5].substr(3);
                o << row[0] << "," << // project id
                     row[1] << "," << // file id
                     row[2] << "," << // total tokens
                     row[3] << "," << // unique tokens
                     "\"" << row[4] << "\",@#@"; // hash
                unsigned i = 5;
                while (true) {
                    std::pair<int, int> x = DecodeTokenMap(row[i]);
                    assert(translation.find(x.first) != translation.end() and "tokens_test and tokenized files differs");
                    o << std::hex << translation[x.first] << "@@::@@" << std::dec << x.second;
                    if (++i == row.size())
                        break;
                    o << ",";
                }
                o << std::endl;
                ++counter;
            });
            Thread::Print(STR("    " << counter << " tokenized files translated" << std::endl));
        }
        /* TODO uncomment this in production, but for now I am keeping the old files as backup */
        Thread::Print(STR("  deleting old files..." << std::endl));
        //exec(STR("rm " << Buffer::FileName(Buffer::Kind::TokensText, tokenizer_)), Writer::OutputDir());
        //exec(STR("rm " << Buffer::FileName(Buffer::Kind::Tokens, tokenizer_)), Writer::OutputDir());
        //exec(STR("rm " << Buffer::FileName(Buffer::Kind::TokenizedFiles, tokenizer_)), Writer::OutputDir());
    }


    void mergeTokenTexts() {
        ClonedProject::StrideIndex() = first_;
        std::string i1 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokensText, tokenizer_));
        ClonedProject::StrideIndex() = second_;
        std::string i2 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokensText, tokenizer_));

        std::ofstream o;
        OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokensText, tokenizer_) << "_" << output_));


        std::unordered_map<Hash, int> tokens;

        // load tokens from the first file
        Thread::Print(STR("  loading token hashes from " << i1 << std::endl));
        firstTokens_ = 0;
        CSVReader::ParseFile(i1, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            Hash h = Hash::Parse(row[2].c_str());
            // add to tokens
            tokens[h] = id;
            // output directly
            o << row[0] << "," << row[1] << ",\"" << row[2] << "\"," << escape(row[3]) << std::endl;
            // increase how many tokens we have seen
            ++firstTokens_;
        });
        int counter = firstTokens_;
        Thread::Print(STR("    " << firstTokens_ << " unique tokens found" << std::endl));

        // now load all tokens from the second one and fill in the translation table
        Thread::Print(STR("  loading & translating token hashes from " << i2 << std::endl));
        unsigned secondTotal = 0;
        unsigned secondUnique = 0;
        CSVReader::ParseFile(i2, [&] (std::vector<std::string> & row) {
            ++secondTotal;
            int id = std::atoi(row[0].c_str());
            Hash h = Hash::Parse(row[2].c_str());
            // if the hash is already known, get the id and do not output it
            auto i = tokens.find(h);
            if (i != tokens.end()) {
                int newId = i->second;
                tokensTranslation_[id] = newId;
            } else {
                ++secondUnique;
                // otherwise obtain new id, and output with the new id
                int newId = counter++;
                tokensTranslation_[id] = newId;
                o << newId << "," << row[1] << ",\"" << row[2] << "\"," << escape(row[3]) << std::endl;
            }
        });
        Thread::Print(STR("    " << secondUnique << " unique tokens found out of " << secondTotal << std::endl));
        Thread::Print(STR("    " << counter << " unique tokens after the merge" << std::endl));
    }

    void mergeTokenUses() {
        ClonedProject::StrideIndex() = first_;
        std::string i1 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::Tokens, tokenizer_));
        ClonedProject::StrideIndex() = second_;
        std::string i2 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::Tokens, tokenizer_));

        std::ofstream o;
        OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::Tokens, tokenizer_) << "_" << output_));

        std::map<int, unsigned> secondUses;
        int reported = 0;
        Thread::Print(STR("  loading token use data from second stride " << i2 << std::endl));
        CSVReader::ParseFile(i2, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            unsigned uses = std::atoi(row[1].c_str());
            assert (tokensTranslation_.find(id) != tokensTranslation_.end() and "token type mismatch");
            int newId = tokensTranslation_[id];
            // if the id is too large, we know it is unique token from second stride and can output it immediately
            if (newId >= firstTokens_) {
                ++reported;
                o << newId << "," << uses << std::endl;
            } else {
                secondUses[newId] = uses;
            }
        });
        Thread::Print(STR("    " << reported << " record reported immediately" << std::endl));
        Thread::Print(STR("    " << secondUses.size() << " records will be summed with first" << std::endl));

        Thread::Print(STR("  loading token use data from first stride and summiong " << i1 << std::endl));
        CSVReader::ParseFile(i1, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            unsigned uses = std::atoi(row[1].c_str());
            auto i = secondUses.find(id);
            if (i != secondUses.end())
                uses += i->second;
            o << id << "," << uses << std::endl;
            ++reported;
        });
        Thread::Print(STR("    " << reported << " unique tokens after the merge" << std::endl));
    }

    /** Merges tokenized files so that only one record per tokens hash will be reported.

      NOTE If we ever experience memory problems this can be rewritten so that we first output everything in the second and then merge in files from the first. This would work just fine and will require only as much memory as a single stride, but when clone groups are also calculated, larger number of files might be affected by the changes and more database searches.
     */
    void mergeTokenizedFiles() {
        ClonedProject::StrideIndex() = first_;
        std::string i1 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokenizedFiles, tokenizer_));
        ClonedProject::StrideIndex() = second_;
        std::string i2 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokenizedFiles, tokenizer_));

        std::ofstream o;
        OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::TokenizedFiles, tokenizer_) << "_" << output_));

        Thread::Print(STR("  reporting all tokenized files from first: " << i1 << std::endl));
        CSVReader::ParseFile(i1, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[1].c_str());
            Hash h = Hash::Parse(row[4].c_str());
            assert(tokenizedFiles_.find(h) == tokenizedFiles_.end() and "token hashes should be unique");
            tokenizedFiles_[h] = id;
            row[5] = row[5].substr(3);
            o << row[0] << "," << // project id
                 row[1] << "," << // file id
                 row[2] << "," << // total tokens
                 row[3] << "," << // unique tokens
                 "\"" << row[4] << "\",@#@"; // hash
            unsigned i = 5;
            while (true) {
                o << row[i];
                if (++i == row.size())
                    break;
                o << ",";
            }
            o << std::endl;
        });
        Thread::Print(STR("    " << tokenizedFiles_.size() << " files from stride " << first_ << std::endl));

        int reported = 0;
        Thread::Print(STR("  translating and reporting files from second: " << i2 << std::endl));
        CSVReader::ParseFile(i2, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[1].c_str());
            Hash h = Hash::Parse(row[4].c_str());
            // if the hash has not yet been found, translate & report the tokenized file
            auto i = tokenizedFiles_.find(h);
            if ( i == tokenizedFiles_.end()) {
                row[5] = row[5].substr(3);
                o << row[0] << "," << // project id
                     row[1] << "," << // file id
                     row[2] << "," << // total tokens
                     row[3] << "," << // unique tokens
                     "\"" << row[4] << "\",@#@"; // hash
                unsigned i = 5;
                while (true) {
                    std::pair<int, int> x = DecodeTokenMap(row[i]);
                    assert(tokensTranslation_.find(x.first) != tokensTranslation_.end() and "tokens_test and tokenized files differs");
                    o << std::hex << tokensTranslation_[x.first] << "@@::@@" << std::dec << x.second;
                    if (++i == row.size())
                        break;
                    o << ",";
                }
                o << std::endl;
                ++reported;
            // otherwise, if there is a file with the same tokens hash already tokenized in the first stride, do not report the second one, but remember the mapping
            } else {
                duplicatesSecondToFirst_[id] = i->second;
            }
        });
        tokenizedFiles_.clear(); // no loger needed
        Thread::Print(STR("    " << reported << " unique tokenized files translated" << std::endl));
        Thread::Print(STR("    " << duplicatesSecondToFirst_.size() << " clone group translations created" << std::endl));
    }

    /** Merges clone pairs of the two strides.
     */
    void mergeClonePairs() {
        ClonedProject::StrideIndex() = first_;
        std::string i1 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::ClonePairs, tokenizer_));
        ClonedProject::StrideIndex() = second_;
        std::string i2 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::ClonePairs, tokenizer_));

        std::ofstream o;
        OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::ClonePairs, tokenizer_) << "_" << output_));

        unsigned reported;

        std::unordered_set<int> cloneGroupsInFirst;

        /* First takes all clone pairs from the first stride and outputs them as they are. We also remember a set of all clone group ids in the first stride along the way.
         */
        Thread::Print(STR("  reporting clone pairs from first: " << i1 << std::endl));
        CSVReader::ParseFile(i1, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            int groupId = std::atoi(row[1].c_str());
            if (id == groupId)
                cloneGroupsInFirst.insert(groupId);
            o << id << "," << groupId << std::endl;
            ++reported;
        });
        Thread::Print(STR("    " << reported << " clone pairs reported" << std::endl));
        Thread::Print(STR("    " << cloneGroupsInFirst.size() << " clone groups detected" << std::endl));

        /* Now take all clone pairs from the second stride.

          We already have a mapping from shared second id's to first id's and we will use this to distinguish different clone group mergings:

          - all records with clone groups from second whose group id is not in the duplicate's map will be left unchanged.
          - all records with clone groups whose id is in duplicates and whose duplicate value is in first clone groups:
              (these are clone groups that exist in both first and second. Clone group id from first will be used)
              - the duplicate's value (group id in first) will be used as group id
              - they will be added to list of sharedCloneGroups
          - all records with clone groups whose id is in duplicates and whose duplicate value is *not* in first clone groups:
              (i.e. a group in second to which a unique file from first should be added)
              - report the clone pair mapping of the first file to the second group
              - add the pair to groupSecondFileFirst
         */

        std::unordered_set<int> accounted; // ids from second that have been accounted for

        Thread::Print(STR("  reporting clone pairs from second: " << i2 << std::endl));
        CSVReader::ParseFile(i2, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            int groupId = std::atoi(row[1].c_str());
            auto i = duplicatesSecondToFirst_.find(groupId);
            // if the group has no duplicate in first, we can just output it
            if (i == duplicatesSecondToFirst_.end()) {
                o << id << "," << groupId << std::endl;
                ++reported;
            } else {
                int firstId = i->second; // get the id in the first stride
                if (cloneGroupsInFirst.find(firstId) != cloneGroupsInFirst.end()) {
                    // if the firstId is a clone group itself, output with the first id
                    o << id << "," <<  firstId << std::endl;
                    ++reported;
                    // the groupId -> groupId mapping is done only once, add to the sharedCloneGroups
                    if (id == groupId) {
                        sharedCloneGroups_[firstId] = id;
                        accounted.insert(id);
                    }
                } else {
                    // remove from the list of ids to be processed, so that next time the group will be assumed to be second only
                    duplicatesSecondToFirst_.erase(i);
                    // report the first file as belonging to the group
                    o << firstId << "," << groupId << std::endl;
                    // report the actual record
                    o << id << "," << groupId << std::endl;
                    reported += 2;
                    // add the id -> firstId to groupSecondFileFirst for cdatemerging
                    groupSecondFileFirst_[id] = firstId;
                }
            }
        });
        // we can now delete all affected duplicates (we could not do that before and we did not want to count on the id == groupid being the last one
        for (int i : accounted)
            duplicatesSecondToFirst_.erase(i);
        accounted.clear();
        Thread::Print(STR("    " << reported << " clone pairs reported in total" << std::endl));
        Thread::Print(STR("    " << sharedCloneGroups_.size() << " clone groups in both first and second" << std::endl));
        Thread::Print(STR("    " << groupSecondFileFirst_.size() << " clone groups in second with single file in first" << std::endl));

        /* Currently the only remaining duplicatesSecondToFirst are firstGroups and second files and first files and second files. We want it to only contain first files and second files so we must find all first groups and second files.
         */
        Thread::Print(STR("  adding second files to first groups" << std::endl));
        for (auto & i : duplicatesSecondToFirst_) {
            int secondId = i.first;
            int firstId = i.second;
            if (cloneGroupsInFirst.find(firstId) != cloneGroupsInFirst.end()) {
                // mark as accounted for, add to groupsFirstFileSecond and report the clone pair
                accounted.insert(secondId);
                o << secondId << "," << firstId << std::endl;
                groupFirstFileSecond_[firstId] = secondId;
            } else {
                // keep in duplcicateSecondToFirst, output clone pair for both, secondId will be the group id
                o << firstId << "," << secondId << std::endl;
                o << secondId << "," << secondId << std::endl;
                reported += 2;
            }
        }
        // remove those accounted for from duplicates so that duplicates only contain file -> file mappings
        for (int i : accounted)
            duplicatesSecondToFirst_.erase(i);
        accounted.clear();
        Thread::Print(STR("    " << reported << " clone pairs reported in total" << std::endl));
        Thread::Print(STR("    " << groupFirstFileSecond_.size() << " clone groups in first with single file in second" << std::endl));
        Thread::Print(STR("    " << duplicatesSecondToFirst_.size() << " single files in both first and second" << std::endl));

    }

    int oldestFrom(SQLConnection & sql, int first, int second) {
        unsigned cdate1 = -1;
        unsigned cdate2 = -1;
        sql.query(STR("SELECT createdAt FROM " << Buffer::TableName(Buffer::Kind::FilesExtra, tokenizer_) << " WHERE fileId = " << first), [&cdate1] (unsigned cols, char ** row) {
            cdate1 = std::atoi(row[0]);
        });
        assert (cdate1 != -1);
        sql.query(STR("SELECT createdAt FROM " << Buffer::TableName(Buffer::Kind::FilesExtra, tokenizer_) << " WHERE fileId = " << second), [&cdate2] (unsigned cols, char ** row) {
            cdate2 = std::atoi(row[0]);
        });
        assert (cdate1 != -2);
        return (cdate1 < cdate2) ? first : second;
    }

    void mergeCloneGroups() {
        ClonedProject::StrideIndex() = first_;
        std::string i1 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::CloneGroups, tokenizer_));
        ClonedProject::StrideIndex() = second_;
        std::string i2 = STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::CloneGroups, tokenizer_));

        std::ofstream o;
        OpenFile(o, STR(Writer::OutputDir() << "/" << Buffer::FileName(Buffer::Kind::CloneGroups, tokenizer_) << "_" << output_));

        SQLConnection sql;
        sql.query(STR("USE " << DBWriter::DatabaseName()));
        int reported = 0;
        int merged = 0;
        std::unordered_map<int, int> firstGroupOldest;

        /* First go through all clone groups in the first stride.

          For each clone group, if it is in sharedCloneGroups, then:
            1) reverse the mapping in sharedCloneGroups so that it gives for second group its first one. The reversal is safe as the first & second ids must be different.
            2) store its oldest file id in the firstGroupOldest. Once we know the second group's oldest we will merge

          If the groupId is in the groupFirstFileSecond, update its oldest file to be the second file if it is oldest and report it

          Otherwise it is a group in first that has no members in second, just output it.
         */

        Thread::Print(STR("  processing clone groups from first: " << i1 << std::endl));
        CSVReader::ParseFile(i1, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            int oldest = std::atoi(row[1].c_str());
            auto i = sharedCloneGroups_.find(id);
            if (i != sharedCloneGroups_.end()) {
                // shared with first and second, reverse mapping, remember oldest and defer processing
                int secondId = i->second;
                sharedCloneGroups_.erase(i);
                sharedCloneGroups_[secondId] = id;
                firstGroupOldest[id] = oldest;
            } else {
                auto i = groupFirstFileSecond_.find(id);
                // if there is a single file in second that belongs to the group, check if it is older than oldest from first and update oldest accordingly
                if (i != groupFirstFileSecond_.end()) {
                    int secondId = i->second;
                    oldest = oldestFrom(sql, oldest, secondId);
                    ++merged;
                }
                // report with the proper oldest (or if unique to first report with own oldest)
                o << id << "," << oldest << std::endl;
                ++reported;
            }
        });
        assert(merged == groupFirstFileSecond_.size());
        groupFirstFileSecond_.clear(); // not needed anymore
        Thread::Print(STR("    " << reported << " clone groups reported in total" << std::endl));
        Thread::Print(STR("    " << merged << " group oldest file information merged" << std::endl));
        Thread::Print(STR("    " << sharedCloneGroups_.size() << " merges deferred" << std::endl));


        /* Now the sharedCloneGroups point from second to first, we have firstGroupOldest to get the oldest for deferred first groups. Process the second clone groups:

          For those in sharedCloneGroups, output their oldest merged with the now known first oldest.

          For those in groupSecondFileFirst, output their oldest with the first id.

          All others are unique to second, just output them as they are.
         */
        merged = 0;
        Thread::Print(STR("  processing clone groups from first: " << i2 << std::endl));
        CSVReader::ParseFile(i2, [&] (std::vector<std::string> & row) {
            int id = std::atoi(row[0].c_str());
            int oldest = std::atoi(row[1].c_str());
            auto i = sharedCloneGroups_.find(id);
            if (i != sharedCloneGroups_.end()) {
                // get oldest file from first group and merge the oldest
                assert(firstGroupOldest.find(i->second) != firstGroupOldest.end());
                int firstOldest = firstGroupOldest[i->second];
                oldest = oldestFrom(sql, oldest, firstOldest);
                ++merged;
            } else {
                auto i = groupSecondFileFirst_.find(id);
                // if there is a single file in first, update oldest acordingly
                if (i != groupSecondFileFirst_.end()) {
                    int firstId = i->second;
                    oldest = oldestFrom(sql, oldest, firstId);
                    ++merged;
                }
            }
            // report with the proper oldest (or if unique to first report with own oldest)
            o << id << "," << oldest << std::endl;
            ++reported;
        });
        assert(merged == sharedCloneGroups_.size() + groupSecondFileFirst_.size());
        sharedCloneGroups_.clear();
        groupSecondFileFirst_.clear();
        firstGroupOldest.clear();
        Thread::Print(STR("    " << reported << " clone groups reported in total" << std::endl));
        Thread::Print(STR("    " << merged << " group oldest file information merged" << std::endl));

        /* All that remains is to deal with single file in first and single file in second, in which case a new clone group is generated (the group id was secondId).
         */
        Thread::Print(STR("  processin new clone groups from single files in both inputs" << std::endl));
        for (auto & i : duplicatesSecondToFirst_) {
            int groupId = i.first;
            int oldest = i.second;
            oldest = oldestFrom(sql, groupId, oldest);
            o << groupId << "," << oldest << std::endl;
            ++reported;
        }
        duplicatesSecondToFirst_.clear();
        Thread::Print(STR("    " << reported << " clone groups reported in total" << std::endl));
    }

    TokenizerKind tokenizer_;

    int first_;
    int second_;
    int output_;

    /** Token id second -> token id in first or new token in in merged
     */
    std::unordered_map<int, int> tokensTranslation_;

    /** For each tokens hash in first -> file id in first.
     */
    std::unordered_map<Hash, int> tokenizedFiles_;

    /** second -> first

      For each tokenized file in second stride that has a tokenized file with the same tokens hash in the first stride contains the file id of the second one as key and the file id as the first one as value.
     */
    std::unordered_map<int, int> duplicatesSecondToFirst_;


    /** first -> second
        (second to first after reversed)

      Any clone group in first that also has clone group in second.
     */
    std::unordered_map<int, int> sharedCloneGroups_;


    /** second -> first

      For a clone group in second to which a single file from first should be added.
     */
    std::unordered_map<int, int> groupSecondFileFirst_;

    /** first -> second

      For clone groups in first to which a single file from second should be added.
     */
    std::unordered_map<int, int> groupFirstFileSecond_;



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
    ResultsMerger m(0, 1, TokenizerKind::Generic);
    m.merge();
}
