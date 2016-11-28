#include "Tokenizer.h"




std::atomic_uint Tokenizer::archives_(0);
std::atomic_uint Tokenizer::utf16be_(0);
std::atomic_uint Tokenizer::utf16le_(0);

std::set<TokenizerKind> Tokenizer::tokenizers_;
