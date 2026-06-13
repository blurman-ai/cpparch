#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "archcheck/rules/violation.h"
#include "archcheck/scan/file_source.h"

namespace archcheck::scan
{

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
// Minimal first cut: no parent-tree comparison yet, so a pre-existing clone
// whose copy the diff merely touched (e.g. reformat) still reports. The
// parent-guard that drops those is the next step (#123 plan).
[[nodiscard]] NewCloneDriftResult detectNewClones(FileSource &newSource, const AddedLineMap &added);

} // namespace archcheck::scan
