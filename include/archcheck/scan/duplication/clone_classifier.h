#pragma once

#include <string>
#include <vector>

#include "archcheck/scan/duplication/fragmenter.h"

namespace archcheck::scan::duplication
{

struct DiffOp
{
  char tag = '='; // '=' equal, '~' changed, '-' delete, '+' insert
  std::string a;
  std::string b;
  int ai = -1; // source index in a (-1 if absent)
  int bj = -1; // source index in b (-1 if absent)
};

// Compute full token-level LCS edit script (full DP table).
// Returns DiffOps with adjacent del+ins collapsed into "changed" (~) ops.
std::vector<DiffOp> diffTokens(const std::vector<std::string> &a, const std::vector<std::string> &b);

// Classify clone type based on how fragments diverge:
//   EXACT      identical token-for-token and spelling
//   RENAMED    only local identifiers differ; normalized stream intact
//   LITERAL    only literals differ; normalized stream intact
//   MIXED      both identifiers and literals differ; normalized stream intact
//   STRUCTURAL normalized streams themselves diverge (Type-3)
const char *cloneType(const Fragment &a, const Fragment &b);

} // namespace archcheck::scan::duplication
