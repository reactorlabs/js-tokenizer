#include <ctime>
#include <cstdlib>
#include <iostream>
#include <map>
#include <unordered_set>
#include <ciso646>
#include <cassert>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <sstream>

#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

#include "hashlib/hashlibpp.h"

#define EXCLUDE_CLONES 1
#define EXCLUDE_MINJS 1



unsigned const N = 32;

std::string const PATH_proj_paths = "../projects.txt";
std::string const PATH_tokens_folder = "tokens";
std::string const PATH_bookkeeping_file_folder = "bookkeeping_files";
std::string const PATH_bookkeeping_proj_folder = "bookkeeping_projs";
std::string const PATH_projects_success = "projects_success.txt";
std::string const PATH_projects_starting_index = "project_startingt_index.txt";
std::string const PATH_projects_fail = "prohjects_fail.txt";

// number of threads
std::atomic_int counter(N);

// sync mutex for writing the results
std::mutex mtx;

class CloneInfo {
public:
    unsigned pid;
    unsigned fid;
    unsigned num;

    CloneInfo() = default;

    CloneInfo(unsigned pid, unsigned fid):
        pid(pid),
        fid(fid),
        num(1) {
    }

    CloneInfo & operator ++ () {
        ++num;
        return *this;
    }
};

std::map<std::string, CloneInfo> hashes;


/** Little class that matches separators.

  A simple matching FSM. Can be made faster if needs be, using tables.
 */
class SeparatorMatcher {
public:

    SeparatorMatcher():
        current_(initial_) {
    }

    /** Advances the current state using given character.

      Returns true if an advancement was possible, false if the matcher had to reset.
     */
    bool match(char c) {
        auto i = current_->next.find(c);
        if (i == current_->next.end()) {
            current_ = initial_;
            i = current_->next.find(c);
            if (i != current_->next.end())
                current_ = i->second;
            return false;
        } else {
            current_ = i->second;
            return true;
        }
    }

    /** Returns the length of matched separator.

      0 if no separator has been found.
     */
    unsigned matched() const {
        return current_->result;
    }

    static void addMatch(std::string const & what) {
        initial_->addMatch(what, 0);
    }

    static void initializeJS() {
        addMatch(" ");
        addMatch("\n");
        addMatch("\r");
        addMatch("\t");
        addMatch(";");
        addMatch("::");
        addMatch(".");
        addMatch(",");
        addMatch("->");
        addMatch("[");
        addMatch("]");
        addMatch("(");
        addMatch(")");
        addMatch("++");
        addMatch("--");
        addMatch("~");
        addMatch("!");
        addMatch("-");
        addMatch("+");
        addMatch("&");
        addMatch("*");
        addMatch(".*");
        addMatch("->*");
        addMatch("/");
        addMatch("%");
        addMatch("<<");
        addMatch(">>");
        addMatch("<");
        addMatch(">");
        addMatch("<=");
        addMatch(">=");
        addMatch("!=");
        addMatch("^");
        addMatch("|");
        addMatch("&&");
        addMatch("||");
        addMatch("?");
        addMatch("==");
        addMatch("{");
        addMatch("}");
        addMatch("=");
        addMatch("#");
        addMatch("\"");
        addMatch("\\");
        addMatch(":");
        addMatch("$");
        addMatch("'");
    }


private:

    class State {
    public:
        unsigned result = 0;
        std::map<char, State *> next;

        void addMatch(std::string const & what, unsigned index) {
            if (index == what.size()) {
                assert(result == 0 and "Ambiguous match should not happen");
                result = index;
            } else {
                auto i = next.find(what[index]);
                if (i == next.end())
                    i = next.insert(std::pair<char, State*>(what[index], new State())).first;
                i->second->addMatch(what, index +1);
            }
        }
    };

    static State * initial_;
    State * current_;

};

SeparatorMatcher::State * SeparatorMatcher::initial_ = new SeparatorMatcher::State();

// output stream for tokenization results
std::ofstream tokens_file;
// file paths bookkeeping
std::ofstream bookkeeping_file;
// project directories bookkeeping
std::ofstream bookkeeping_proj;

// total files tokenized
std::atomic_uint total_files(0);
// empty or error files (skipped)
std::atomic_uint empty_files(0);
// minjs files (skipped)
std::atomic_uint minjs_files(0);

// total clones found
unsigned total_clones = 0;

// project id
unsigned project_id = 0;
// last known duration in [s] (for once per second stats)
unsigned lastDuration = 0;
// total number of parsed bytes
unsigned long long bytes = 0;

// start of the tokenization for timing purposes
std::chrono::high_resolution_clock::time_point start;


/** Tokenizes a JS file in single pass.
 */
class Tokenizer {
public:

    /** Opens the given file and tokenizes it into the current output in the sourcerer format.
     */
    static bool tokenize(std::string const & filename, std::ostream & output, unsigned pid, unsigned fid) {
        std::ifstream input(filename);
        if (not input.good()) {
            // std::cerr << "Unable to open file " << filename << std::endl;
            return false;
        }
        Tokenizer t(input, pid, fid, filename);
        t.tokenize();
        if (not t.empty())
            return t.writeCounts(output);
        else
            return false;
    }

private:

    enum class State {
        Ready,
        Comment1,
        LineComment,
        MultiComment,
        MultiCommentMayEnd,
    };

    Tokenizer(std::istream & file, unsigned pid, unsigned fid, std::string const & filename):
        f_(file),
        pid_(pid),
        fid_(fid),
        filename_(filename) {
    }

    /** The main loop deals with comments, ignoring tokens inside them and moving between normal state, line comments and multi-line comments.
     */
    void tokenize() {
        State state = State::Ready;
        while (true) {
            char c = f_.get();
            if (f_.eof())
                break;
            ++bytes_;
            switch (state) {
                case State::Ready:
                    if (c == '/')
                        state = State::Comment1;
                    checkToken(c);
                    break;
                case State::Comment1:
                    if (c == '/')
                        state = State::LineComment;
                    else if (c == '*')
                        state = State::MultiComment;
                    else
                        state = State::Ready;
                    checkToken(c);
                    break;
                case State::LineComment:
                    if (c == '\n') {
                        state = State::Ready;
                        reset();
                    }
                    break;
                case State::MultiComment:
                    if (c == '*')
                        state = State::MultiCommentMayEnd;
                    break;
                case State::MultiCommentMayEnd:
                    if (c == '/') {
                        state = State::Ready;
                        reset();
                    } else {
                        state = State::MultiComment;
                    }
                    break;
            }
        }
        // TODO we must add token if there is last one too
        if (not current_.empty() and not inSeparator_)
            ++tokens_[current_];
    }

    /** With given new character, checks whether separator has been matched, in which case the token parsed so far should be accounted. Then flips into separator matching phase (so that we correctly match the longest available separator (i.e. a== will trigger after first =, but must parse the second one correctly as well.
     */
    void checkToken(char c) {
        current_ += c;
        if (not matcher_.match(c) and inSeparator_)
            inSeparator_ = false;
        unsigned matchedLength = matcher_.matched();
        if (matchedLength > 0) {
            if (not inSeparator_) {
                std::string token;
                for (size_t i = 0, e = current_.length() - matchedLength; i != e; ++i) {
                    unsigned char c = current_[i];
                    if (c < ' ' or c > '~') {
                        token += "\\";
                        token += ('0' + (c / 64));
                        c = c % 64;
                        token += '0' + (c / 8);
                        c = c % 8;
                        token += '0' + c;
                    } else if (c == '\\') {
                        token += "\\\\";
                    } else if (c == '@') {
                        token += "\\@";
                    } else {
                        token += c;
                    }
                }
                if (not token.empty()) {
                    // starts at 0
                    ++tokens_[token];
                    ++totalTokens_;
                }
                inSeparator_ = true;
            }
            current_.clear();
        }
    }

    /** Resets the tokenizer so that it can accept new tokens.
     */
    void reset() {
        current_.clear();
        inSeparator_ = false;
    }

    /** Gets the tokens string, formatted as they should be in the output file.
     */
    std::string tokensString() const {
        // TODO this may not be the most efficient way to do it
        std::stringstream ss;
        for (auto i: tokens_)
            ss << "," << i.first << "@@::@@" << i.second;
        return ss.str();
    }

    /** Writes the token counts into the output stream using the sourcerer's notation.

      The format with metadata is:

      `project id`, `file id`, `total tokens in file`, `unique tokens`, `md5 hash`, `@#@` tokens
     */
    bool writeCounts(std::ostream & output) const {
        bool result = true;
        std::string s(tokensString());
        // get the md5
        md5wrapper md5;
        std::string hash = md5.getHashFromString(s);
        mtx.lock();
        // check the hash for clone
#if EXCLUDE_CLONES == 1
        auto i = hashes.find(hash);
        if (i != hashes.end()) {
            ++total_clones;
            ++(i->second);
            result = false;
        } else {
            hashes[hash] = CloneInfo(pid_, fid_);
            output << pid_ << "," << fid_ << "," << totalTokens_ << "," << tokens_.size() << "," << hash << "@#@" << s << std::endl;
            bookkeeping_file << pid_ << "," << fid_ << "," << filename_ << std::endl;
            bytes += bytes_;
        }
#else
        output << pid_ << "," << fid_ << "," << totalTokens_ << "," << tokens_.size() << "," << hash << "@#@" << s << std::endl;
        bookkeeping_file << pid_ << "," << fid_ << "," << filename_ << std::endl;
        bytes += bytes_;
#endif
        mtx.unlock();
        return result;
    }

    bool empty() {
        return tokens_.empty();
    }

    // counted tokens
    std::map<std::string, unsigned> tokens_;

    // input
    std::istream & f_;

    // currently parsed token
    std::string current_;

    // continued separator matching after token addition
    bool inSeparator_ = false;

    // matcher used for separator detection
    SeparatorMatcher matcher_;

    long long bytes_ = 0;

    long totalTokens_ = 0;

    unsigned pid_;
    unsigned fid_;
    std::string const & filename_;
};


/** Prints statistics about bitrate and processed files.
 */
void printStats() {
    auto now = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count();
    unsigned d = ms / 1000;
    if (d != lastDuration) {
        std::cout << "Processed files " << total_files << ", throughput " << (bytes / (ms / 1000) / 1024 / 1024) << "[MB/s]" << std::endl;
        lastDuration = d;
    }
}

/** Simple helper that checks whether string ends with given characters.
 */
bool endsWith(std::string const & str, std::string const & suffix) {
   if (str.length() >= suffix.length()) {
       return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
   } else {
       return false;
   }
}

/** Tokenizes all javascript files in the directory. Recursively searches in subdirectories.
 */
void tokenizeDirectory(DIR * dir, std::string path, unsigned pid, unsigned & fid) {
    struct dirent * ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
            continue;
        std::string p = path + "/" + ent->d_name;
        DIR * d = opendir(p.c_str());
        if (d != nullptr) {
            tokenizeDirectory(d, p, pid, fid);
        } else if (endsWith(ent->d_name, ".js")) {
#if EXCLUDE_MINJS == 1
            // skip min.js files
            if (endsWith(ent->d_name, ".min.js"))  {
                ++minjs_files; // atomic
                continue;
            }
#endif
            std::string filename = path + "/" + ent->d_name;

            if (Tokenizer::tokenize(filename, tokens_file, pid, fid)) {
                ++fid; // local
                ++total_files; // atomic
            } else {
                ++empty_files;
            }
        }
    }
    closedir(dir);
}

/** Resets the file id, then walks the project directory recursively and analyzes all its JS files.

  Finally updates the project bookkeeping file with project id and location.
 */
void tokenizeProject(DIR * dir, std::string path, unsigned pid) {
    unsigned fid = 0;
    tokenizeDirectory(dir, path, pid, fid = 0);
}

/** Initializes output directories if not yet created.
 */
void initializeDirectories() {
    /** Initialize directories if they do not exist */
    mkdir(PATH_tokens_folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir(PATH_bookkeeping_file_folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir(PATH_bookkeeping_proj_folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

/** Opens the tokens and bookkeeping streams with unique id attached.
 */
void openUniqueStreams() {
    unsigned result = 0;
    while (true) {
        ++result;
        std::string num = std::to_string(result);
        std::ifstream f(PATH_tokens_folder + "/tokens_" + num + ".txt");
        if (f.good())
            continue;
        f.open(PATH_bookkeeping_file_folder + "/bookkeeping_file_" + num + ".txt");
        if (f.good())
            continue;
        f.open(PATH_bookkeeping_file_folder + "/bookkeeping_proj_" + num + ".txt");
        if (f.good())
            continue;
        break;
    }
    std::string num = std::to_string(result);
    std::cout << "This run ID is: " << num << std::endl;
    tokens_file.open(PATH_tokens_folder + "/tokens_" + num + ".txt");
    bookkeeping_file.open(PATH_bookkeeping_file_folder + "/bookkeeping_file_" + num + ".txt");
    bookkeeping_proj.open(PATH_bookkeeping_proj_folder + "/bookkeeping_proj_" + num + ".txt");
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cerr << "Invalid usage! You must specify root folder where to look for the projects";
        return EXIT_FAILURE;
    }

    // initialize
    SeparatorMatcher::initializeJS();
    initializeDirectories();
    openUniqueStreams();

    // start timing
    start = std::chrono::high_resolution_clock::now();

    // walk the root directory and find projects
    std::string rootPath = argv[1];
    DIR * root = opendir(rootPath.c_str());
    if (root == nullptr) {
        std::cerr << "Unable to open root directory " << rootPath << std::endl;
        return EXIT_FAILURE;
    }
    struct dirent * ent;
    std::cout << "Workers " << N << std::endl;
    while ((ent = readdir(root)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
            continue;
        std::string p = rootPath + "/" + ent->d_name;
        DIR * d = opendir(p.c_str());
        if (d != nullptr) {
            unsigned pid = ++project_id;
            if (N == 1) {
                tokenizeProject(d, p, pid);
                printStats();
            } else {
                // stupid wait, should be fine...
                while (counter == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    printStats();
                }
                    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
                // decrement counter for we will spawn a worker
                --counter;
                // write project bookkeeping so that we do it in main thread
                bookkeeping_proj << pid << "," << p << std::endl;
                std::thread t([d, p, pid] () {
                    tokenizeProject(d, p, pid); // shoudl the start take too long
                    ++counter;
                });
                t.detach();
            }


        }
    }
    while (counter != N)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    closedir(root);

    // print final stats
    auto now = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    double tf = total_clones + empty_files + minjs_files + total_files;

    std::cout << "Total files visited:  " << unsigned(tf) << std::endl;
    tf = tf / 100;
    std::cout << "Empty or error files: " << empty_files << " " << empty_files / tf << "% (skipped)"  << std::endl;
#if EXCLUDE_MINJS == 1
    std::cout << "Minified files:       " << minjs_files << " " << minjs_files / tf << "% (skipped)"  << std::endl;
#endif
#if EXCLUDE_CLONES == 1
    std::cout << "Exact clones found:   " << total_clones << " " << total_clones / tf << "%" << std::endl;
#endif
    std::cout << "Total bytes:          " << (bytes / (double) 1024 / 1024) << "[MB]" << std::endl;
    std::cout << "Total time:           " << (ms/1000) << " [s]" << std::endl;
    std::cout << "Processed " << total_files << " (" << total_files / tf << " %) files in " << project_id << " projects, throughput " << (bytes / (ms / 1000) / 1024 / 1024) << "[MB/s]" << std::endl;

    return EXIT_SUCCESS;
}
