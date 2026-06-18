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

// Content identity of a fragment: hash of its normalized token sequence. Stable
// across reformat/whitespace churn, which is what lets the parent-guard recognise
// a touched-but-pre-existing clone.
std::string fragKey(const duplication::Fragment &f)
{
  std::string joined;
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

} // namespace

NewCloneDriftResult detectNewClones(const SourceSnapshot &newSnapshot, const SourceSnapshot &parentSnapshot,
                                    const AddedLineMap &added)
{
  NewCloneDriftResult result;
  if (added.empty())
    return result;
  const auto parentKeys = parentPairKeys(parentSnapshot);
  const auto sources = newSnapshot.authoredSources();
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

} // namespace archcheck::scan
