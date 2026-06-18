#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "archcheck/rules/violation.h"

namespace archcheck::scan
{

class SourceSnapshot;

// New-file line numbers a commit added, keyed by repo-relative path. Built by
// the caller from git::collectAddedLines — keeps this module free of git/.
using AddedLineMap = std::unordered_map<std::string, std::unordered_set<int>>;

// Clones a commit *introduces* (#123). Advisory only — never gates the exit code.
struct NewCloneDriftResult
{
  rules::ViolationList violations;
};

// Scan the whole new tree for duplication, then keep only pairs where at least
// one side overlaps a line the diff added — i.e. copy-paste this commit brought
// in. Scanning the whole tree (not just changed files) is deliberate: the twin
// of a freshly added clone often lives in an unchanged file.
//
// Parent-guard: a pair whose clone relationship already existed in the parent
// snapshot is dropped, so a pre-existing clone whose copy the diff merely touched
// (e.g. a reformat) does not report. Pair identity is the normalized token
// sequence of both sides, so whitespace/reformat churn doesn't defeat the guard.
// A brand-new copy of pre-existing code still fires: the parent had only one
// instance, hence no pair to match against.
//
// Consumes pre-read+classified snapshots (#129): authored files only, no reading.
[[nodiscard]] NewCloneDriftResult detectNewClones(const SourceSnapshot &newSnapshot,
                                                  const SourceSnapshot &parentSnapshot, const AddedLineMap &added);

} // namespace archcheck::scan
