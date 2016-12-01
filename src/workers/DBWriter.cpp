#include "DBWriter.h"


std::string SQLConnection::server_;
std::string SQLConnection::user_;
std::string SQLConnection::pass_;




std::string const DBWriter::TableStamp = "stamp";
std::string const DBWriter::TableProjects = "projects";
std::string const DBWriter::TableProjectsExtra = "projects_extra";
std::string const DBWriter::TableFiles = "files";
std::string const DBWriter::TableFilesExtra = "files_extra";
std::string const DBWriter::TableStats = "stats";
std::string const DBWriter::TableClonePairs = "clone_pairs";
std::string const DBWriter::TableCloneGroups = "clone_groups";
std::string const DBWriter::TableTokensText = "tokens_text";
std::string const DBWriter::TableTokens = "tokens";
std::string const DBWriter::TableTokenHashes = "token_hashes";
std::string const DBWriter::TableCloneGroupHashes = "clone_group_hashes";
std::string const DBWriter::TableSummary = "summary";


std::string DBWriter::db_;

