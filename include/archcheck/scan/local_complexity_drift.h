#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "archcheck/rules/violation.h"

namespace archcheck::scan
{

class SourceSnapshot;

// Result of the baseline/current local-complexity comparison for changed files.
// Advisory only: never gates the exit code (#101).
struct ComplexityDriftResult
{
  rules::ViolationList violations;
  int positiveDelta = 0; // sum of positive per-function deltas (PR aggregate)
  int negativeDelta = 0; // sum of negative deltas — reported as improvement
};

// Compare per-function cognitive complexity between the two versions of one
// file. Matching key is (qualifiedName, paramArity); ambiguous matches degrade
// to nearest-start-line with low confidence and are excluded from LCX.1/LCX.2.
// GoogleTest/benchmark macro bodies (TEST_F etc.) are ignored.
[[nodiscard]] ComplexityDriftResult compareLocalComplexity(const std::string &oldSource, const std::string &newSource,
                                                           const std::string &file);

// Run the comparison over the changed C/C++ files of a diff, taking both versions
// from the pre-read+classified snapshots (#129). A changed file's authored verdict
// comes from the whole current tree, so vendored/test/generated/banner files are
// dropped with the same classification as graph and clone.
[[nodiscard]] ComplexityDriftResult detectLocalComplexityDrift(const SourceSnapshot &oldSnapshot,
                                                               const SourceSnapshot &newSnapshot,
                                                               const std::vector<std::filesystem::path> &changedFiles);

} // namespace archcheck::scan
