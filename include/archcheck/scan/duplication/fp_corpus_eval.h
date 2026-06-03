#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "archcheck/scan/duplication/duplication_scanner.h"

namespace archcheck::scan::duplication
{

// Evaluation metrics for duplication detector against fp_corpus_r2.tsv ground truth.
struct CorpusMetrics
{
  std::size_t truePositives = 0;  // TP: real clones kept
  std::size_t falsePositives = 0; // FP: false positives kept
  std::size_t falseNegatives = 0; // FN: real clones missed
  std::size_t trueNegatives = 0;  // TN: non-clones correctly rejected
  double precision = 0.0;         // TP / (TP + FP)
  double recall = 0.0;            // TP / (TP + FN)

  std::unordered_map<std::string, int> fpByClass; // FP count per class (idiom, data-table, etc)
};

// Load ground truth from experiments/verification/fp_corpus_r2.tsv.
// Returns map: (repo, sha, line_a, line_b) → label (TP or FP), class name.
// On parse error, returns empty map.
using GroundTruth = std::unordered_map<std::string, std::pair<std::string, std::string>>;
GroundTruth loadCorpusGroundTruth(const std::string &corpusPath);

// Evaluate detector output against ground truth.
// Pairs are keyed by (file_a, line_a, file_b, line_b) for matching.
CorpusMetrics evaluateAgainstCorpus(const ScanResult &result, const GroundTruth &groundTruth);

} // namespace archcheck::scan::duplication
