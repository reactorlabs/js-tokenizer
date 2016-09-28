#include "utils.h"

#include "writer.h"

std::ostream & operator << (std::ostream & s, WriterJob const & job) {
    s << job.file->absPath();
    return s;
}

std::string Writer::outputDir_;

Writer::Writer(unsigned index):
    QueueWorker<WriterJob>(STR("WRITER " << index)) {
    openStreamAndCheck(files_, STR(outputDir_ << "/" << PATH_STATS_FILE << "/" << STATS_FILE << index << STATS_FILE_EXT));
    openStreamAndCheck(projs_, STR(outputDir_ << "/" << PATH_BOOKKEEPING_PROJS << "/" << BOOKKEEPING_PROJS << index << BOOKKEEPING_PROJS_EXT));
    openStreamAndCheck(tokens_, STR(outputDir_ << "/" << PATH_TOKENS_FILE << "/" << TOKENS_FILE << index << TOKENS_FILE_EXT));
    openStreamAndCheck(clones_, STR(outputDir_ << "/" << PATH_CLONES_FILE << "/" << CLONES_FILE << index << CLONES_FILE_EXT));
    openStreamAndCheck(fullStats_, STR(outputDir_ << "/" << PATH_FULL_STATS_FILE << "/" << FULL_STATS_FILE << index << FULL_STATS_FILE_EXT));

}


void Writer::initializeOutputDirectory(std::string const & output) {
    outputDir_ = output;
    if (isDirectory(output))
        Worker::Warning(STR("Output directory " << output << " already exists, data may be corrupted"));
    createDirectory(output + "/" + PATH_STATS_FILE);
    createDirectory(output + "/" + PATH_BOOKKEEPING_PROJS);
    createDirectory(output + "/" + PATH_TOKENS_FILE);
    createDirectory(output + "/" + PATH_CLONES_FILE);
    createDirectory(output + "/" + PATH_FULL_STATS_FILE);
}


void Writer::openStreamAndCheck(std::ofstream & s, std::string const & filename) {
    s.open(filename);
    if (not s.good())
        throw STR("Unable to open file " << filename << " for writing");
}


void Writer::process(WriterJob const & job) {
    // always output full stats
    job.file->stats.writeFullStats(fullStats_);
    // if not empty and not clone, output sourcererCC's info
    if (not job.file->empty() and not job.isClone()) {
        job.file->stats.writeSourcererStats(files_);
        job.file->writeTokens(tokens_);

    }
    // finally check if the project should be written as well







}
