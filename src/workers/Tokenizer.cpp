#include "Tokenizer.h"




std::atomic_uint Tokenizer::archives_(0);
std::atomic_uint Tokenizer::binary_(0);
std::atomic_uint Tokenizer::utf16be_(0);
std::atomic_uint Tokenizer::utf16le_(0);
std::atomic_uint Tokenizer::totalFiles_(0);
std::atomic_ulong Tokenizer::totalBytes_(0);


/*Buffer Tokenizer::files_(Buffer::Target::DB, Buffer::Kind::Files);
Buffer Tokenizer::filesExtra_(Buffer::Target::DB, Buffer::Kind::FilesExtra);
*/

std::set<TokenizerKind> Tokenizer::tokenizers_;
