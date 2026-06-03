#include "archcheck/scan/duplication/duplication_scanner.h"

#include <algorithm>
#include <cctype>
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
    double score = scoreOf(p);
    if (score >= opts.simThreshold)
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

// --- P0.9 generated-file FP guard ------------------------------------------
// Machine-generated files (protobuf *.pb.cc, Qt moc_/ui_/qrc_, flex/bison
// output) are never hand-edited, so duplication in them is not actionable
// copy-paste — there is no human to refactor. Recognized by path alone.
//
// NOTE: the platform-twin (P0.7) and perf-variant (P0.8) guards that once lived
// here were REMOVED. They keyed on file PATH and suppressed whole file-pairs,
// but platform/variant files mix genuinely-divergent bodies (correctly NOT
// flagged) with byte-identical helpers like POSIX OS::sleep/OS::truncateFile
// shared across os_macos.cpp/os_linux.cpp — and those identical helpers are
// real, consolidatable duplication (TP). A path guard cannot tell them apart,
// so it was killing true positives. Identical code is reported regardless of path.
std::string toLowerCopy(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// P0.9: generated files — protobuf / Qt-moc / flex-bison output, never hand-edited.
bool isGeneratedPath(const std::string &lowerPath)
{
  static const std::vector<std::string> kMarkers = {".pb.h",       ".pb.cc",      ".pb.cpp", "_generated.",
                                                    ".generated.", "/generated/", "/moc_",   "/ui_",
                                                    "/qrc_",       ".tab.c",      "lex.yy",  ".g.cpp"};
  for (const auto &m : kMarkers)
  {
    if (lowerPath.find(m) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

// P0.9 driver: drop pairs where either file is machine-generated.
void phasePathBasedFpSuppress(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  std::vector<Pair> filtered;
  for (const auto &p : candidates)
  {
    if (isGeneratedPath(toLowerCopy(allFragments[p.a].file)) || isGeneratedPath(toLowerCopy(allFragments[p.b].file)))
    {
      continue; // generated output — not actionable copy-paste
    }
    filtered.push_back(p);
  }
  candidates = std::move(filtered);
}

// P1.1: data-table / literal-run classifier — down-weight pairs that look like data tables
// Rows of case/init patterns with mostly literals (case X: return Y; or vec.push_back({a,b,c}))
// are structural boilerplate, not real logic duplication.
void phase10DataTableClassifier(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  // Heuristic: if high overlap BUT low diversity (too many identical trigrams),
  // likely a data table (switch cases, init lists) rather than logic clone
  for (auto &p : candidates)
  {
    const Fragment &fa = allFragments[p.a];
    const Fragment &fb = allFragments[p.b];

    // Low diversity = many repeated token patterns (case tables, init loops)
    // Reduce score slightly for such pairs
    if (fa.diversity < 0.30 && fb.diversity < 0.30)
    {
      // Down-weight by reducing similarity slightly
      p.weighted *= 0.85; // 15% penalty for table-like structure
    }
  }
}

// P1.2: boilerplate-density filter — drop pairs with mostly non-substantive lines
// Fragments dominated by guard clauses, setter chains, or empty structure are boilerplate
void phase11BoilerplateDensity(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  std::vector<Pair> filtered;

  for (const auto &p : candidates)
  {
    const Fragment &fa = allFragments[p.a];
    const Fragment &fb = allFragments[p.b];

    // Very short fragments (<5 lines) are often boilerplate (constructors, getters)
    // Require higher similarity for them
    if (fa.tokenCount < 20 && fb.tokenCount < 20)
    {
      if (p.weighted >= 0.90)
      {
        filtered.push_back(p); // Only keep very high-confidence short matches
      }
      // else: drop short, low-confidence matches (boilerplate)
    }
    else
    {
      filtered.push_back(p); // Keep longer fragments
    }
  }
  candidates = std::move(filtered);
}

// P1.3: header-impl gate — suppress pairs where one is mostly declarations
// Headers with >70% declaration lines (virtual, override, access specifiers) are interface defs
void phase12HeaderImplGate(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  // For now, keep all pairs (full implementation would count declaration tokens)
  // Heuristic: files ending in .h are headers; pairs between .h and .cpp are expected
  (void)allFragments; // Placeholder implementation
  (void)candidates;
}

// P1.4: file-local IDF down-weight — reduce weight of tokens that are very common in the file
// High-frequency tokens (getters, setters, common names) shouldn't dominate the score
void phase13FileLoclalIDFDownweight(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  // Count token frequencies per file to build per-file stop-list
  std::unordered_map<std::string, std::unordered_map<std::string, int>> fileTokenFreq;

  for (const auto &frag : allFragments)
  {
    for (const auto &[token, count] : frag.bag)
    {
      fileTokenFreq[frag.file][token] += count;
    }
  }

  // For each pair, apply IDF penalty if shared tokens are very file-local
  for (auto &p : candidates)
  {
    const Fragment &fa = allFragments[p.a];
    const Fragment &fb = allFragments[p.b];

    // If both fragments in same file AND tokens are very frequent in that file,
    // down-weight the pair (likely idiom, not real clone)
    if (fa.file == fb.file)
    {
      auto &freq = fileTokenFreq[fa.file];
      std::size_t totalTokens = 0;
      for (const auto &[token, count] : fa.bag)
      {
        if (freq[token] > 10)
          totalTokens += count; // Count high-frequency tokens
      }

      if (totalTokens > fa.tokenCount / 2)
      {
        // More than half tokens are file-local frequent
        p.weighted *= 0.80; // 20% penalty for file-local idiom
      }
    }
  }
}

// Canonical "fileA\nfileB" key (order-independent) for a cross-file pair.
std::string filerPairKey(const std::string &x, const std::string &y) { return x < y ? x + "\n" + y : y + "\n" + x; }

// Set of file-pairs that are whole-file clones: ≥80% of the smaller file's
// fragments (and it has ≥2) are matched across the pair → move/copy/vendored twin.
std::unordered_set<std::string> wholeFileClonePairs(const std::vector<Pair> &candidates,
                                                    const std::vector<Fragment> &allFragments)
{
  std::unordered_map<std::string, int> fragsPerFile;
  for (const auto &f : allFragments)
  {
    ++fragsPerFile[f.file];
  }

  std::unordered_map<std::string, int> matched;
  for (const auto &p : candidates)
  {
    const std::string &fa = allFragments[p.a].file;
    const std::string &fb = allFragments[p.b].file;
    if (fa != fb)
    {
      ++matched[filerPairKey(fa, fb)];
    }
  }

  std::unordered_set<std::string> result;
  for (const auto &[k, n] : matched)
  {
    const std::string::size_type sep = k.find('\n');
    const int minFrags = std::min(fragsPerFile[k.substr(0, sep)], fragsPerFile[k.substr(sep + 1)]);
    if (minFrags >= 2 && n * 5 >= minFrags * 4)
    {
      result.insert(k);
    }
  }
  return result;
}

// P0.2: whole-file clone suppression — count file-level clones (move/copy/vendored
// twins) separately and drop their constituent pairs from the actionable set.
std::size_t phase8WholeFileSuppress(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  const std::unordered_set<std::string> wholeFile = wholeFileClonePairs(candidates, allFragments);
  if (wholeFile.empty())
  {
    return 0;
  }

  std::vector<Pair> filtered;
  for (const auto &p : candidates)
  {
    const std::string &fa = allFragments[p.a].file;
    const std::string &fb = allFragments[p.b].file;
    if (fa != fb && wholeFile.count(filerPairKey(fa, fb)) > 0)
    {
      continue;
    }
    filtered.push_back(p);
  }
  candidates = std::move(filtered);
  return wholeFile.size();
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

// Tag each surviving pair with its clone type (Type-1 EXACT … Type-3 STRUCTURAL).
// Informational only — does not gate; the type rides along to the reporter.
void phaseClassifyCloneType(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments)
{
  for (auto &p : candidates)
  {
    p.type = cloneType(allFragments[p.a], allFragments[p.b]);
  }
}
// Ordered candidate-filtering pipeline (sort → canon → coordinate revalidation →
// P0/P1 guards → clone-type classification), run in place. Split out of
// scanForDuplication so each stays within the complexity budget.
void applyCandidateFilters(std::vector<Pair> &candidates, const std::vector<Fragment> &allFragments, ScanResult &result,
                           const ScannerOptions &opts)
{
  phase4Sort(candidates, opts);
  phase5SymmetricPairCanon(candidates);
  phase6CoordinateRevalidation(candidates, allFragments);
  phase7SameFunctionFilter(candidates, allFragments);
  if (opts.enableJointFloor)
  {
    phase8JointTokenOrderFloor(candidates, opts.jointWeightedThreshold, opts.jointLineThreshold);
  }
  if (opts.enableWholeFileGuard)
  {
    result.wholeFileClones = phase8WholeFileSuppress(candidates, allFragments);
  }
  phase9FunctionBoundaryAnchor(candidates, allFragments);
  // P0.7-P0.9: path-based suppression of intentional/generated duplication
  if (opts.enablePathGuards)
  {
    phasePathBasedFpSuppress(candidates, allFragments);
  }
  // P1 classifiers (optional, can reduce recall if miscalibrated)
  if (opts.enableP1Guards)
  {
    phase10DataTableClassifier(candidates, allFragments);
    phase11BoilerplateDensity(candidates, allFragments);
    phase12HeaderImplGate(candidates, allFragments);
    phase13FileLoclalIDFDownweight(candidates, allFragments);
  }
  phaseClassifyCloneType(candidates, allFragments);
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
  result.scoredCandidateCount = candidates.size(); // Count after similarity gate
  applyCandidateFilters(candidates, allFragments, result, opts);
  result.pairs = candidates;
  return result;
}

} // namespace archcheck::scan::duplication
