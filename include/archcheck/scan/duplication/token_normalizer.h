#pragma once

#include <string>
#include <vector>

namespace archcheck::scan::duplication
{

struct Token
{
  std::string sym; // normalized symbol ("id", "lit", keyword, or operator)
  int line = 0;    // 1-based source line
  std::string raw; // original spelling when sym is placeholder; empty => sym is raw
  int off = 0;     // byte offset of the token start in the source (adjacency checks)
};

// Tokenize C++20 source code with optional selective normalization.
// keepCalls: if true, preserve identifier names when they appear as function
//   callees (id before "(") or case labels. Reduces coincidental matches.
//   if false, all identifiers become "id".
std::vector<Token> lex(const std::string &source, bool keepCalls = false);

} // namespace archcheck::scan::duplication
