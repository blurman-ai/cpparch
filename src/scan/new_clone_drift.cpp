#include "archcheck/scan/new_clone_drift.h"

#include <functional>
#include <string>
#include <unordered_set>

#include "archcheck/scan/duplication/duplication_scanner.h"
#include "archcheck/scan/source_snapshot.h"

namespace archcheck::scan
{

namespace
{

constexpr const char *kRuleId = "DRIFT.NEW_CLONE";

// Location-aware identity of a fragment: its file plus the hash of its normalized
// token sequence. Including the file is what lets the parent-guard tell apart "this
// PR reformatted a copy that already existed here" (same file+content → suppress)
// from "this PR pasted a new copy into a new location" (new file → a real new clone,
// even if the same content was already duplicated elsewhere). The token hash stays
// stable across reformat/whitespace churn, so the reformat case is still recognised.
std::string fragKey(const duplication::Fragment &f)
{
  std::string joined = f.file;
  joined.push_back('\x1f');
  for (const auto &t : f.seq)
  {
    joined += t;
    joined.push_back('\x1f');
  }
  return std::to_string(std::hash<std::string>{}(joined));
}

// Order-independent key for a clone pair, so parent and new trees match regardless
// of which side the scanner happened to label `a`.
std::string pairKey(const duplication::Fragment &a, const duplication::Fragment &b)
{
  const auto ka = fragKey(a);
  const auto kb = fragKey(b);
  return ka < kb ? ka + "|" + kb : kb + "|" + ka;
}

// Clone pairs that already exist in the parent tree, keyed by content. A pair in
// this set was not introduced by the diff, even if the diff touched one side.
std::unordered_set<std::string> parentPairKeys(const SourceSnapshot &parentSnapshot)
{
  const auto sources = parentSnapshot.authoredSources();
  duplication::ScannerOptions opts;
  opts.enableWholeFileGuard = false;
  const auto scan = duplication::scanForDuplication(sources, opts);
  std::unordered_set<std::string> keys;
  for (const auto &p : scan.pairs)
    keys.insert(pairKey(scan.fragments[p.a], scan.fragments[p.b]));
  return keys;
}

// True if any line of [f.startLine, f.endLine] is among the commit's added
// lines in f.file.
bool touchesAdded(const duplication::Fragment &f, const AddedLineMap &added)
{
  const auto it = added.find(f.file);
  if (it == added.end())
    return false;
  for (int ln = f.startLine; ln <= f.endLine; ++ln)
    if (it->second.count(ln))
      return true;
  return false;
}

rules::Violation makeViolation(const duplication::Fragment &introduced, const duplication::Fragment &source,
                               const duplication::Pair &p)
{
  rules::Violation v;
  v.ruleId = kRuleId;
  v.file = introduced.file;
  v.line = introduced.startLine;
  v.message = "copy-paste introduced (" + p.type + "): clone of " + source.file + ":" +
              std::to_string(source.startLine) + "-" + std::to_string(source.endLine);
  return v;
}

// #149: total authored bytes — the whole-tree clone scan cost scales with this.
std::size_t authoredBytes(const std::vector<std::pair<std::string, std::string>> &sources)
{
  std::size_t bytes = 0;
  for (const auto &s : sources)
    bytes += s.second.size();
  return bytes;
}

// Scan the new tree and emit one violation per introduced clone pair: a pair where
// one side touches an added line and the parent did not already contain it.
NewCloneDriftResult emitNewClones(const std::vector<std::pair<std::string, std::string>> &sources,
                                  const std::unordered_set<std::string> &parentKeys, const AddedLineMap &added)
{
  NewCloneDriftResult result;
  // Whole-file guard off: in a snapshot a whole-file duplicate is vendored noise,
  // but a commit that adds a file-copy is exactly the signal we want. Precision
  // filters (joint floor, P1 classifiers) stay on.
  duplication::ScannerOptions opts;
  opts.enableWholeFileGuard = false;
  const auto scan = duplication::scanForDuplication(sources, opts);
  for (const auto &p : scan.pairs)
  {
    const auto &fa = scan.fragments[p.a];
    const auto &fb = scan.fragments[p.b];
    const bool aTouched = touchesAdded(fa, added);
    const bool bTouched = touchesAdded(fb, added);
    if (!aTouched && !bTouched)
      continue;
    if (parentKeys.count(pairKey(fa, fb)))
      continue; // pre-existing clone, diff merely touched it
    result.violations.push_back(aTouched ? makeViolation(fa, fb, p) : makeViolation(fb, fa, p));
  }
  return result;
}

} // namespace

NewCloneDriftResult detectNewClones(const SourceSnapshot &newSnapshot, const SourceSnapshot &parentSnapshot,
                                    const AddedLineMap &added, std::size_t maxScanBytes)
{
  NewCloneDriftResult result;
  if (added.empty())
    return result;
  const auto sources = newSnapshot.authoredSources();
  // #149: the clone scan is a whole-tree pass twice over (parent + new); on a huge
  // authored tree it blows the per-commit budget. Skip past the cap — advisory only,
  // so the gate (cycles/god-headers) still runs and the commit is not lost.
  if (authoredBytes(sources) > maxScanBytes)
  {
    result.skippedLargeTree = true;
    return result;
  }
  return emitNewClones(sources, parentPairKeys(parentSnapshot), added);
}

} // namespace archcheck::scan
