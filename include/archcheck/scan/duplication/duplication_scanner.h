#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "archcheck/scan/duplication/clone_index.h"
#include "archcheck/scan/duplication/fragmenter.h"
#include "archcheck/scan/duplication/similarity.h"

namespace archcheck::scan::duplication
{

struct Pair
{
  std::size_t a = 0;          // first fragment index
  std::size_t b = 0;          // second fragment index
  double weighted = 0.0;      // weighted Jaccard similarity
  double plain = 0.0;         // plain Jaccard similarity
  double line = 0.0;          // line-based overlap
  double lcs = 0.0;           // token-LCS Dice ratio
  std::size_t sharedRare = 0; // count of shared rare tokens
};

struct ScannerOptions
{
  FragmentOptions fragmentOpts;
  IndexOptions indexOpts;
  std::string metric = "weighted";        // "weighted" or "plain" — which metric gates similarity
  bool precise = false;                   // if true, use token-LCS as gate; else use metric
  double simThreshold = 0.60;             // similarity gate threshold
  bool enableJointFloor = true;           // P0.6: require BOTH token AND line metrics to pass
  double jointWeightedThreshold = 0.75;   // P0.6: minimum weighted similarity when joint floor enabled
  double jointLineThreshold = 0.50;       // P0.6: minimum line overlap when joint floor enabled
  bool enableP1Guards = true;             // P1: enable classifier filters (data-table, boilerplate, header-impl, IDF)
};

struct ScanResult
{
  std::vector<Fragment> fragments;
  std::vector<Pair> pairs;
  CloneIndex index;
  std::size_t fileCount = 0;
  std::size_t candidateCount = 0;      // Raw candidates before scoring
  std::size_t scoredCandidateCount = 0; // After similarity gate
  std::size_t totalLoc = 0;
};

// Scan C++ source files and detect duplication: lex → fragment → index → candidate pairs → score.
// Returns all fragments, candidate pairs passing the similarity gate, and index metadata.
ScanResult scanForDuplication(const std::vector<std::pair<std::string, std::string>> &files,
                              const ScannerOptions &opts = ScannerOptions{});

} // namespace archcheck::scan::duplication
