#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "archcheck/rules/violation.h"
#include "archcheck/scan/file_source.h"

namespace archcheck::scan
{

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

// Run the comparison over the changed C/C++ files of a diff, reading both
// versions from the given sources. Vendored and test paths are skipped — same
// classification the corpus run used (file_classification.h).
[[nodiscard]] ComplexityDriftResult detectLocalComplexityDrift(FileSource &oldSource, FileSource &newSource,
                                                               const std::vector<std::filesystem::path> &changedFiles);

} // namespace archcheck::scan
