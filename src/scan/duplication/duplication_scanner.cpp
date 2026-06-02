#include "archcheck/scan/duplication/duplication_scanner.h"

#include <algorithm>
#include <unordered_set>

#include "archcheck/scan/duplication/clone_classifier.h"
#include "archcheck/scan/duplication/token_normalizer.h"

namespace archcheck::scan::duplication
{

namespace
{
void phase1TokenizeAndExtract(const std::vector<std::pair<std::string, std::string>> &files, const ScannerOptions &opts,
                              std::vector<Fragment> &allFragments, std::size_t &totalLoc)
{
  for (const auto &[path, source] : files)
  {
    const auto tokens = lex(source, opts.fragmentOpts.minTokens > 0);
    const auto fragments = extractFragments(tokens, source, path, opts.fragmentOpts);
    for (const auto &frag : fragments)
    {
      totalLoc += frag.endLine - frag.startLine + 1;
      allFragments.push_back(frag);
    }
  }
}

std::vector<Pair> phase3ScoreCandidates(const std::vector<Fragment> &allFragments, const CloneIndex &index,
                                        const ScannerOptions &opts)
{
  auto scoreOf = [&opts](const Pair &p) -> double
  { return opts.precise ? p.lcs : (opts.metric == "plain" ? p.plain : p.weighted); };

  std::vector<Pair> candidates;
  for (const auto &[pr, shared] : index.sharedRare)
  {
    Pair p;
    p.a = pr.first;
    p.b = pr.second;
    p.sharedRare = shared;
    p.weighted = weightedJaccard(allFragments[p.a], allFragments[p.b], index.idf);
    p.plain = plainJaccard(allFragments[p.a], allFragments[p.b]);
    p.line = lineOverlap(allFragments[p.a], allFragments[p.b]);
    if (opts.precise)
    {
      p.lcs = lcsRatio(allFragments[p.a], allFragments[p.b]);
    }
    if (scoreOf(p) >= opts.simThreshold)
    {
      candidates.push_back(p);
    }
  }
  return candidates;
}

void phase4Sort(std::vector<Pair> &candidates, const ScannerOptions &opts)
{
  auto scoreOf = [&opts](const Pair &p) -> double
  { return opts.precise ? p.lcs : (opts.metric == "plain" ? p.plain : p.weighted); };
  std::sort(candidates.begin(), candidates.end(),
            [&](const Pair &l, const Pair &r) { return scoreOf(l) > scoreOf(r); });
}

// P0.5: symmetric-pair canonicalization — deduplicate (a,b) and (b,a), keep a < b
void phase5SymmetricPairCanon(std::vector<Pair> &candidates)
{
  std::unordered_set<std::string> seen;
  std::vector<Pair> deduped;

  for (const auto &p : candidates)
  {
    std::string canonical = std::to_string(std::min(p.a, p.b)) + "," + std::to_string(std::max(p.a, p.b));
    if (seen.find(canonical) == seen.end())
    {
      seen.insert(canonical);
      deduped.push_back(p);
    }
  }
  candidates = std::move(deduped);
}

// P0.3: coordinate revalidation (simplified) — filter phantom ranges
// Drops pairs where fragment line ranges are invalid (startLine < 1)
void phase6CoordinateRevalidation(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  std::vector<Pair> filtered;

  for (const auto &p : candidates)
  {
    const Fragment &fa = allFragments[p.a];
    const Fragment &fb = allFragments[p.b];

    // Sanity check: both fragments must have valid line ranges
    if (fa.startLine > 0 && fa.endLine >= fa.startLine && fb.startLine > 0 && fb.endLine >= fb.startLine)
    {
      filtered.push_back(p);
    }
  }
  candidates = std::move(filtered);
}

// P0.1: diff-hunk + blame attribution (simplified) — filter same-function/overlapping spans
// In diff-mode, this would check git hunks; here we use a heuristic:
// drop same-file pairs that overlap or are immediately adjacent (likely internal idiom, not clone)
void phase7SameFunctionFilter(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  std::vector<Pair> filtered;

  for (const auto &p : candidates)
  {
    const Fragment &fa = allFragments[p.a];
    const Fragment &fb = allFragments[p.b];

    // Different files: always keep (can't determine function boundary without AST)
    if (fa.file != fb.file)
    {
      filtered.push_back(p);
      continue;
    }

    // Same file: check for overlap or adjacent ranges
    // If ranges don't overlap and aren't immediately adjacent, keep the pair
    bool overlapping = (fa.startLine <= fb.endLine && fb.startLine <= fa.endLine);
    bool adjacent = (fa.endLine + 1 == fb.startLine || fb.endLine + 1 == fa.startLine);

    if (!overlapping && !adjacent)
    {
      filtered.push_back(p);
    }
  }
  candidates = std::move(filtered);
}

// P0.6: joint token∧order floor — require high similarity in BOTH metrics
// Don't emit pairs where token-weight is high but line-overlap is low (bag-of-words collision)
// or vice versa. Tuning: thresholds calibrated on fp_corpus_r2.tsv.
void phase8JointTokenOrderFloor(std::vector<Pair> &candidates, double minWeighted = 0.75, double minLine = 0.50)
{
  std::vector<Pair> filtered;

  for (const auto &p : candidates)
  {
    // Both metrics must be satisfied: w >= minWeighted AND line >= minLine
    // This rejects high-token-weight + low-line-sim pairs (idiom collisions)
    if (p.weighted >= minWeighted && p.line >= minLine)
    {
      filtered.push_back(p);
    }
  }
  candidates = std::move(filtered);
}
} // namespace

ScanResult scanForDuplication(const std::vector<std::pair<std::string, std::string>> &files, const ScannerOptions &opts)
{
  ScanResult result;
  std::vector<Fragment> allFragments;

  phase1TokenizeAndExtract(files, opts, allFragments, result.totalLoc);
  result.fileCount = files.size();
  result.fragments = allFragments;

  if (allFragments.empty())
  {
    return result;
  }

  result.index = buildIndex(allFragments, opts.indexOpts);
  result.candidateCount = result.index.sharedRare.size();

  std::vector<Pair> candidates = phase3ScoreCandidates(allFragments, result.index, opts);
  phase4Sort(candidates, opts);
  phase5SymmetricPairCanon(candidates);
  phase6CoordinateRevalidation(candidates, allFragments);
  phase7SameFunctionFilter(candidates, allFragments);
  if (opts.enableJointFloor)
  {
    phase8JointTokenOrderFloor(candidates, opts.jointWeightedThreshold, opts.jointLineThreshold);
  }

  result.pairs = candidates;
  return result;
}

} // namespace archcheck::scan::duplication
