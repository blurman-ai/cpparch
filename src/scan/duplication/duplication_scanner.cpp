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

  result.pairs = candidates;
  return result;
}

} // namespace archcheck::scan::duplication
