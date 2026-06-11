#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "archcheck/git/history_query.h"
#include "archcheck/rules/rule_set.h"

namespace archcheck::scan
{

// God-file growth detector: finds files that are large and growing consistently.
// Takes the repository's commit history and current LOC per file, returns
// violations for files matching all conditions:
// 1. Current LOC >= P75 (75th percentile of all production files)
// 2. Net growth >= +30% OR >= +300 lines
// 3. >= 5 consecutive recent commits with added > deleted
// 4. No commits with deleted - added >= 50 in the history window
class GodFileGrowthDetector
{
public:
  // currentLocMap: path -> line count in current tree (from current filesystem scan)
  // history: commits in chronological order (oldest first, as returned by queryCommitHistory)
  explicit GodFileGrowthDetector(const std::map<std::string, std::int32_t> &currentLocMap,
                                 const std::vector<archcheck::git::CommitStats> &history);

  // Run the detector, return violations for god-file growth candidates.
  [[nodiscard]] archcheck::rules::ViolationList detect() const;

private:
  std::map<std::string, std::int32_t> currentLocMap_;
  std::vector<archcheck::git::CommitStats> history_;

  // Calculate P75 of current LOC for production files (non-vendor, non-test).
  [[nodiscard]] std::int32_t calculateP75() const;

  // Check a single file against all four conditions.
  [[nodiscard]] bool isGodFileCandidate(std::string_view path, std::int32_t currentLoc, std::int32_t p75) const;

  // Helper: return net lines added for a file across the entire history window.
  [[nodiscard]] std::int32_t computeNetGrowth(std::string_view path) const;

  // Helper: count consecutive recent commits where added > deleted for a file.
  [[nodiscard]] std::int32_t countConsecutiveGrowthTouches(std::string_view path) const;

  // Helper: check if any commit has deleted - added >= 50 for a file.
  [[nodiscard]] bool hasMeaningfulShrink(std::string_view path) const;

  // Helper: format violation message.
  [[nodiscard]] std::string formatMessage(std::int32_t currentLoc, std::int32_t netGrowth,
                                          std::int32_t growthTouches) const;
};

} // namespace archcheck::scan
