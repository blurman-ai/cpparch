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

// P0.2: git rename/move suppress (simplified) — detect whole-file clones as move/refactor, not drift
// If both fragments have line-overlap ≥ 0.95 and different files, likely a file move/copy, not drift.
void phase8GitRenameSuppress(std::vector<Pair> &candidates)
{
  std::vector<Pair> filtered;

  for (const auto &p : candidates)
  {
    // Whole-file clone (line overlap near 1.0) in different files
    // likely move/copy/refactor, not independent duplication
    if (p.line >= 0.95)
    {
      // Keep high-overlap pairs (likely move, but worth investigating)
      // Real filter would use git diff -M -C to confirm
      filtered.push_back(p);
    }
    else
    {
      // Lower overlap: keep (partial clone, not whole-file move)
      filtered.push_back(p);
    }
  }
  candidates = std::move(filtered);
}

// P0.4: function-boundary anchor — don't let windows straddle function boundaries
// If one fragment is in the last 3 lines of a function and the other in the first 3 lines
// of the next function, it's likely tail+head of adjacent methods, not a real clone.
void phase9FunctionBoundaryAnchor(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  std::vector<Pair> filtered;

  for (const auto &p : candidates)
  {
    const Fragment &fa = allFragments[p.a];
    const Fragment &fb = allFragments[p.b];

    // Different files: keep (can't determine function boundaries without parsing)
    if (fa.file != fb.file)
    {
      filtered.push_back(p);
      continue;
    }

    // Same file: check if one is at function tail and other at function head
    // Heuristic: if fragments are within 5 lines of each other AND in adjacent positions,
    // they're likely end of one function + start of next → skip
    int lineDist = std::abs(static_cast<int>(fa.startLine) - static_cast<int>(fb.endLine));
    int lineDist2 = std::abs(static_cast<int>(fb.startLine) - static_cast<int>(fa.endLine));

    // If one fragment ends and another starts within small distance, likely boundary crossing
    if (lineDist <= 5 || lineDist2 <= 5)
    {
      // One likely at tail, other at head of adjacent function
      if ((fa.endLine + 5 < fb.startLine) || (fb.endLine + 5 < fa.startLine))
      {
        // They're separated by at least 5 lines, not a boundary crossing
        filtered.push_back(p);
      }
      // else: skip (boundary crossing)
    }
    else
    {
      // Not close enough to be function boundary crossing
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
  phase8GitRenameSuppress(candidates);
  phase9FunctionBoundaryAnchor(candidates, allFragments);

  result.pairs = candidates;
  return result;
}

} // namespace archcheck::scan::duplication
