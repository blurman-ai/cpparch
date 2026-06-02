#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "archcheck/scan/duplication/token_normalizer.h"

namespace archcheck::scan::duplication
{

struct Fragment
{
  std::string file;
  int startLine = 0;
  int endLine = 0;
  std::size_t tokenCount = 0;
  std::unordered_map<std::string, int> bag;  // normalized token -> count
  std::vector<std::string> seq;              // ordered normalized tokens (for LCS)
  std::vector<std::string> rawSeq;           // raw spelling per token, aligned with seq
  std::unordered_set<std::string> normLines; // illustrative line-based view
  double diversity = 1.0;                    // distinct-trigram ratio (low = skeletal)
};

struct FragmentOptions
{
  std::size_t minTokens = 30;
  std::size_t maxTokens = 400;
};

// Extract function-scale fragments from tokenized source.
// A "{" preceded by ")" is a function/control body; we emit such blocks if
// they fall within [minTokens, maxTokens]. Otherwise we descend to find
// inner bodies recursively. The source text is needed to extract source lines.
std::vector<Fragment> extractFragments(const std::vector<Token> &tokens, const std::string &source,
                                       const std::string &file, const FragmentOptions &opts = FragmentOptions{});

} // namespace archcheck::scan::duplication
