#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace archcheck::scan::duplication
{

struct Fragment;

struct IndexOptions
{
  std::size_t rareDfCap = 4;     // document frequency cap for "rare" tokens
  double rareDfPct = 0.0;        // if >0, rare cutoff = max(rareDfCap, N*pct/100)
  std::size_t minSharedRare = 2; // minimum rare tokens to form a candidate pair
  double minDiversity = 0.0;     // min trigram diversity; filter skeletal fragments
};

struct CloneIndex
{
  std::unordered_map<std::string, int> df;                               // token -> document frequency
  std::unordered_map<std::string, double> idf;                           // token -> IDF weight
  std::unordered_map<std::string, std::vector<std::size_t>> postings;    // token -> fragment indices
  std::map<std::pair<std::size_t, std::size_t>, std::size_t> sharedRare; // (fi, fj) -> shared rare token count
};

// Build inverted index from fragments: compute df/idf, postings, and candidate pairs.
CloneIndex buildIndex(const std::vector<Fragment> &fragments, const IndexOptions &opts = IndexOptions{});

} // namespace archcheck::scan::duplication
