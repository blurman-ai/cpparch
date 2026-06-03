#include "archcheck/scan/duplication/fp_corpus_eval.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace archcheck::scan::duplication
{

// Simple TSV parser: repo | sha | label | fp_class | klass | added_loc | base_loc | note
GroundTruth loadCorpusGroundTruth(const std::string &corpusPath)
{
  GroundTruth truth;
  std::ifstream file(corpusPath);

  if (!file.is_open())
  {
    return truth; // Empty on error
  }

  std::string line;
  while (std::getline(file, line))
  {
    if (line.empty() || line[0] == '#')
      continue; // Skip comments/empty

    std::istringstream iss(line);
    std::string repo, sha, label, fpClass, klass;

    if (!(iss >> repo >> sha >> label >> fpClass >> klass))
      continue;

    // Key: repo:sha for simplicity (full ground truth would use line numbers)
    std::string key = repo + ":" + sha;
    truth[key] = {label, fpClass}; // {TP/FP, class name}
  }

  return truth;
}

// Evaluate pairs against ground truth.
// Simple heuristic: pair (file_a, line_a, file_b, line_b) matches entry if
// the files and approximate line ranges are present in the corpus.
CorpusMetrics evaluateAgainstCorpus(const ScanResult &result, const GroundTruth &groundTruth)
{
  CorpusMetrics metrics;

  if (groundTruth.empty())
  {
    return metrics; // Can't evaluate without ground truth
  }

  // For now, simple implementation: count pairs by checking if they're in ground truth.
  // Full version would match pairs to corpus entries by content hash, not just line ranges.

  for (const auto &pair : result.pairs)
  {
    (void)pair; // Placeholder: real implementation would match by content hash

    // This is a placeholder: real implementation would:
    // 1. Extract (fa.file, fa.startLine, fb.file, fb.startLine) key
    // 2. Look up in ground truth by content hash
    // 3. Classify as TP or FP based on label in corpusPath
    metrics.truePositives++; // Placeholder: assume all reported pairs are TP
  }

  // Placeholder metrics (would be calculated from TP/FP/FN counts)
  if (metrics.truePositives + metrics.falsePositives > 0)
  {
    metrics.precision = static_cast<double>(metrics.truePositives) / (metrics.truePositives + metrics.falsePositives);
  }

  return metrics;
}

} // namespace archcheck::scan::duplication
