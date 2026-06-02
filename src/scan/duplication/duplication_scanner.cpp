#include "archcheck/scan/duplication/duplication_scanner.h"

#include <algorithm>

#include "archcheck/scan/duplication/clone_classifier.h"
#include "archcheck/scan/duplication/token_normalizer.h"

namespace archcheck::scan::duplication
{

ScanResult scanForDuplication(const std::vector<std::pair<std::string, std::string>> &files, const ScannerOptions &opts)
{
  ScanResult result;
  std::vector<Fragment> allFragments;

  // Phase 1: Tokenize and extract fragments from each file
  for (const auto &[path, source] : files)
  {
    const auto tokens = lex(source, opts.fragmentOpts.minTokens > 0);
    const auto fragments = extractFragments(tokens, source, path, opts.fragmentOpts);

    for (const auto &frag : fragments)
    {
      result.totalLoc += frag.endLine - frag.startLine + 1;
      allFragments.push_back(frag);
    }
  }

  result.fileCount = files.size();
  result.fragments = allFragments;

  if (allFragments.empty())
  {
    return result;
  }

  // Phase 2: Build inverted index and find candidate pairs
  result.index = buildIndex(allFragments, opts.indexOpts);
  result.candidateCount = result.index.sharedRare.size();

  // Phase 3: Score candidates and filter by threshold
  auto scoreOf = [&opts](const Pair &p) -> double
  { return opts.precise ? p.lcs : (opts.metric == "plain" ? p.plain : p.weighted); };

  std::vector<Pair> candidates;
  for (const auto &[pr, shared] : result.index.sharedRare)
  {
    Pair p;
    p.a = pr.first;
    p.b = pr.second;
    p.sharedRare = shared;
    p.weighted = weightedJaccard(allFragments[p.a], allFragments[p.b], result.index.idf);
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

  // Phase 4: Sort by score
  std::sort(candidates.begin(), candidates.end(),
            [&](const Pair &l, const Pair &r) { return scoreOf(l) > scoreOf(r); });

  result.pairs = candidates;
  return result;
}

} // namespace archcheck::scan::duplication
