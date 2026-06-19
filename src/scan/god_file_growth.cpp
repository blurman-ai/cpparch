#include "archcheck/scan/god_file_growth.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

#include "archcheck/scan/file_classification.h"

namespace archcheck::scan
{

namespace
{

// Thresholds from spec
constexpr std::int32_t kMinGrowthPercent = 30;          // +30%
constexpr std::int32_t kMinGrowthAbsolute = 300;        // +300 lines
constexpr std::int32_t kMinConsecutiveGrowth = 5;       // >= 5 commits
constexpr std::int32_t kMeaningfulShrinkThreshold = 50; // deleted - added >= 50

} // namespace

GodFileGrowthDetector::GodFileGrowthDetector(const std::map<std::string, std::int32_t> &currentLocMap,
                                             const std::vector<archcheck::git::CommitStats> &history)
    : currentLocMap_(currentLocMap), history_(history)
{
}

std::int32_t GodFileGrowthDetector::calculateP75() const
{
  std::vector<std::int32_t> productionLocs;
  for (const auto &[path, loc] : currentLocMap_)
  {
    // Exclude vendored-directory and test files from the p75 baseline. (Generated
    // files and curated vendored basenames are NOT excluded here yet — see #127.)
    if (pathHasVendoredDir(path) || pathHasTestDir(path) || isTestBasename(baseName(path)))
    {
      continue;
    }
    productionLocs.push_back(loc);
  }

  if (productionLocs.empty())
  {
    return 0;
  }

  std::sort(productionLocs.begin(), productionLocs.end());
  const std::size_t idx = static_cast<std::size_t>(std::ceil(productionLocs.size() * 0.75)) - 1;
  return productionLocs[idx];
}

std::int32_t GodFileGrowthDetector::computeNetGrowth(std::string_view path) const
{
  std::int32_t totalAdded = 0;
  std::int32_t totalDeleted = 0;
  for (const auto &commit : history_)
  {
    for (const auto &file : commit.files)
    {
      if (file.path == path)
      {
        totalAdded += file.added;
        totalDeleted += file.deleted;
      }
    }
  }
  return totalAdded - totalDeleted;
}

std::int32_t GodFileGrowthDetector::countConsecutiveGrowthTouches(std::string_view path) const
{
  // Count from the most recent commits backwards until we hit a non-growth commit.
  // Commits are stored oldest-first, so iterate backwards.
  std::int32_t count = 0;
  for (auto it = history_.rbegin(); it != history_.rend(); ++it)
  {
    const auto &commit = *it;
    bool hasFile = false;
    bool isGrowth = false;
    for (const auto &file : commit.files)
    {
      if (file.path == path)
      {
        hasFile = true;
        isGrowth = (file.added > file.deleted);
        break;
      }
    }
    // If this commit touches the file and it's growth, increment count
    if (hasFile && isGrowth)
    {
      count++;
    }
    else if (hasFile && !isGrowth)
    {
      // Touched but not growth; stop the consecutive count
      break;
    }
    // If hasFile is false, continue looking backwards (commit didn't touch this file)
  }
  return count;
}

bool GodFileGrowthDetector::hasMeaningfulShrink(std::string_view path) const
{
  for (const auto &commit : history_)
  {
    for (const auto &file : commit.files)
    {
      if (file.path == path && (file.deleted - file.added) >= kMeaningfulShrinkThreshold)
      {
        return true;
      }
    }
  }
  return false;
}

std::string GodFileGrowthDetector::formatMessage(std::int32_t currentLoc, std::int32_t netGrowth,
                                                 std::int32_t growthTouches) const
{
  std::ostringstream os;
  os << "current " << currentLoc << " LOC, net +" << netGrowth << " lines, " << growthTouches
     << " consecutive growth commits";
  return os.str();
}

bool GodFileGrowthDetector::isGodFileCandidate(std::string_view path, std::int32_t currentLoc, std::int32_t p75) const
{
  // Condition 1: current LOC >= P75
  if (currentLoc < p75)
  {
    return false;
  }

  // Condition 2: net growth >= +30% OR >= +300 lines
  const auto netGrowth = computeNetGrowth(path);
  const bool growthPercent = (netGrowth >= (currentLoc * kMinGrowthPercent / 100));
  const bool growthAbsolute = (netGrowth >= kMinGrowthAbsolute);
  if (!growthPercent && !growthAbsolute)
  {
    return false;
  }

  // Condition 3: >= 5 consecutive recent growth touches
  const auto consecutiveGrowth = countConsecutiveGrowthTouches(path);
  if (consecutiveGrowth < kMinConsecutiveGrowth)
  {
    return false;
  }

  // Condition 4: no meaningful shrink (deleted - added >= 50)
  if (hasMeaningfulShrink(path))
  {
    return false;
  }

  return true;
}

archcheck::rules::ViolationList GodFileGrowthDetector::detect() const
{
  archcheck::rules::ViolationList violations;
  const auto p75 = calculateP75();

  for (const auto &[path, currentLoc] : currentLocMap_)
  {
    // Skip vendored-directory and test files (same gate as the p75 baseline above).
    if (pathHasVendoredDir(path) || pathHasTestDir(path) || isTestBasename(baseName(path)))
    {
      continue;
    }

    if (isGodFileCandidate(path, currentLoc, p75))
    {
      const auto netGrowth = computeNetGrowth(path);
      const auto growthTouches = countConsecutiveGrowthTouches(path);
      archcheck::rules::Violation v;
      v.ruleId = "SIZE.1.god_file_growth";
      v.file = path;
      v.line = 0;
      v.message = formatMessage(currentLoc, netGrowth, growthTouches);
      violations.push_back(v);
    }
  }

  return violations;
}

} // namespace archcheck::scan
