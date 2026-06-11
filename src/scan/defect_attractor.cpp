#include "archcheck/scan/defect_attractor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>
#include <string>

#include "archcheck/rules/violation.h"
#include "archcheck/scan/file_classification.h"

namespace archcheck::scan
{

namespace
{

constexpr std::int32_t kMinFixTouches = 5; // Absolute floor for a finding

// Pattern for fix-like keywords (case-insensitive).
// Matches word boundaries to exclude prefix/suffix/postfix/fixture.
const std::regex kFixPattern(R"(\b(fix(es|ed)?|bug|crash|regress(ion|ed)?|hotfix|segfault|fault|oops|leak)\b)",
                             std::regex::icase);

bool isProductionFile(std::string_view path)
{
  // Skip vendor, test, and generated code
  if (pathHasVendoredDir(path) || pathHasTestDir(path) || isTestBasename(baseName(path)))
  {
    return false;
  }

  // Check if file has a project extension
  for (std::string_view ext : kProjectExtensions)
  {
    if (path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
    {
      return true;
    }
  }

  return false;
}

} // namespace

DefectAttractorDetector::DefectAttractorDetector(const std::vector<archcheck::git::CommitStats> &history)
    : history_(history)
{
}

bool DefectAttractorDetector::isFixLikeCommit(std::string_view subject)
{
  // Check if subject matches the fix-like pattern (case-insensitive).
  std::string subjectStr(subject);
  return std::regex_search(subjectStr, kFixPattern);
}

bool DefectAttractorDetector::shouldSkipCommit(const archcheck::git::CommitStats &commit)
{
  // Skip merge commits
  if (commit.subject.size() >= 5 && commit.subject.substr(0, 5) == "Merge")
  {
    return true;
  }

  // Skip commits with empty file list (merge commits without -m)
  if (commit.files.empty())
  {
    return true;
  }

  // Skip mechanical fixes: fix-like AND > 30 files touched
  const bool isFixLike = isFixLikeCommit(commit.subject);
  if (isFixLike && commit.files.size() > 30)
  {
    return true;
  }

  return false;
}

std::map<std::string, std::int32_t> DefectAttractorDetector::aggregateFixTouches() const
{
  std::map<std::string, std::int32_t> fixTouches;

  for (const auto &commit : history_)
  {
    // Skip merge commits, empty file lists, and mechanical fixes
    if (shouldSkipCommit(commit))
    {
      continue;
    }

    // Check if this is a fix-like commit
    if (!isFixLikeCommit(commit.subject))
    {
      continue;
    }

    // Count this commit for each production file it touches
    for (const auto &file : commit.files)
    {
      if (isProductionFile(file.path))
      {
        fixTouches[file.path]++;
      }
    }
  }

  return fixTouches;
}

std::int32_t DefectAttractorDetector::calculateTopDecileThreshold(const std::map<std::string, std::int32_t> &fixTouches)
{
  if (fixTouches.empty())
  {
    return 0;
  }

  // Extract all touch counts
  std::vector<std::int32_t> counts;
  for (const auto &[path, count] : fixTouches)
  {
    if (count > 0)
    {
      counts.push_back(count);
    }
  }

  if (counts.empty())
  {
    return 0;
  }

  // If fewer than 10 files, top-decile = top-1 (max)
  if (counts.size() < 10)
  {
    return *std::max_element(counts.begin(), counts.end());
  }

  // Sort and compute 90th percentile
  std::sort(counts.begin(), counts.end());
  const std::size_t idx = static_cast<std::size_t>(std::ceil(counts.size() * 0.90)) - 1;
  return counts[idx];
}

std::string DefectAttractorDetector::formatMessage(std::int32_t fixTouches) const
{
  std::ostringstream os;
  os << fixTouches << " fix-like touches in " << history_.size() << " commits";
  return os.str();
}

namespace
{

std::vector<std::pair<std::string, std::int32_t>>
filterCandidates(const std::map<std::string, std::int32_t> &fixTouches, std::int32_t threshold)
{
  std::vector<std::pair<std::string, std::int32_t>> candidates;
  for (const auto &[path, count] : fixTouches)
  {
    if (count >= kMinFixTouches && count >= threshold)
    {
      candidates.push_back({path, count});
    }
  }
  std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
  return candidates;
}

} // namespace

archcheck::rules::ViolationList DefectAttractorDetector::detect() const
{
  archcheck::rules::ViolationList violations;

  const auto fixTouches = aggregateFixTouches();
  if (fixTouches.empty())
  {
    return violations;
  }

  const auto threshold = calculateTopDecileThreshold(fixTouches);
  if (threshold == 0)
  {
    return violations;
  }

  const auto candidates = filterCandidates(fixTouches, threshold);
  for (const auto &[path, count] : candidates)
  {
    archcheck::rules::Violation v;
    v.ruleId = "HIST.1.defect_attractor";
    v.file = path;
    v.line = 0;
    v.message = formatMessage(count);
    violations.push_back(v);
  }

  return violations;
}

} // namespace archcheck::scan
