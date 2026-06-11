#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "archcheck/git/history_query.h"
#include "archcheck/rules/rule_set.h"

namespace archcheck::scan
{

// Detects files that are disproportionately frequent in fix-like commits
// ("defect attractors"). Analyzes git history for commits with fix-related
// keywords in their subject lines and flags files that appear in many such
// commits relative to the peer population.
class DefectAttractorDetector
{
public:
  // Constructs detector from git history (newest commits first).
  explicit DefectAttractorDetector(const std::vector<archcheck::git::CommitStats> &history);

  // Detect and return violations for files flagged as defect attractors.
  // A file is flagged if it appears in fix-like commits >= 5 times and is
  // in the top decile (or top-1 if fewer than 10 files have any fix touches).
  [[nodiscard]] archcheck::rules::ViolationList detect() const;

private:
  const std::vector<archcheck::git::CommitStats> &history_;

  // Returns true if commit subject matches fix-like patterns (case-insensitive).
  // Pattern: \b(fix(es|ed)?|bug|crash|regress(ion|ed)?|hotfix|segfault|fault|oops|leak)\b
  [[nodiscard]] static bool isFixLikeCommit(std::string_view subject);

  // Returns true if commit should be skipped:
  // - empty file list (merge commits without -m)
  // - subject starts with "Merge"
  // - fix-like AND files.size() > 30 (mechanical/batch fix)
  [[nodiscard]] static bool shouldSkipCommit(const archcheck::git::CommitStats &commit);

  // Count fix-like touches for each production file (not vendor/test/generated).
  // Returns map of path -> fix_count.
  [[nodiscard]] std::map<std::string, std::int32_t> aggregateFixTouches() const;

  // Determine top-decile threshold from fix-touch distribution.
  // If fewer than 10 files with fixes exist, threshold = max.
  [[nodiscard]] static std::int32_t calculateTopDecileThreshold(const std::map<std::string, std::int32_t> &fixTouches);

  // Format violation message with fix count and total commits in window.
  [[nodiscard]] std::string formatMessage(std::int32_t fixTouches) const;
};

} // namespace archcheck::scan
