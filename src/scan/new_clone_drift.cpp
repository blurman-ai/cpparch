#include "archcheck/scan/new_clone_drift.h"

#include <string>

#include "archcheck/scan/duplication/duplication_scanner.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::scan
{

namespace
{

constexpr const char *kRuleId = "DRIFT.NEW_CLONE";

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

NewCloneDriftResult detectNewClones(FileSource &newSource, const AddedLineMap &added)
{
  NewCloneDriftResult result;
  if (added.empty())
    return result;
  const auto sources = collectNonVendoredSources(newSource);
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
    result.violations.push_back(aTouched ? makeViolation(fa, fb, p) : makeViolation(fb, fa, p));
  }
  return result;
}

} // namespace archcheck::scan
