#pragma once

#include <filesystem>
#include <stdexcept>

#include "archcheck/rules/violation.h"

namespace archcheck::report
{

struct BaselineError : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct ViolationBaseline
{
  rules::ViolationList known;
};

// Serialise baseline to JSON file. Throws BaselineError on I/O failure.
void saveBaseline(const ViolationBaseline &baseline, const std::filesystem::path &path);

// Deserialise baseline from JSON file written by saveBaseline.
// Throws BaselineError if the file cannot be opened or is not a valid archcheck baseline.
ViolationBaseline loadBaseline(const std::filesystem::path &path);

// Return only violations in `all` whose (ruleId, file, line) triple is absent from baseline.
rules::ViolationList filterNew(const rules::ViolationList &all, const ViolationBaseline &baseline);

} // namespace archcheck::report
