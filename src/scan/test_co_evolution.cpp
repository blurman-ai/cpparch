#include "archcheck/scan/test_co_evolution.h"

#include <filesystem>
#include <sstream>
#include <string>

#include "archcheck/scan/file_classification.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::scan
{

namespace
{

// Classify a path into one of three buckets: production, test, or other.
// Rename pairs are identified by presence of " => " and classified by their destination.
enum class PathBucket
{
  Production, // has project extension, not test, not vendor
  Test,       // has project extension, is test (dir or basename)
  Other       // everything else
};

PathBucket classifyPath(const std::string &path)
{
  // Rename handling: extract destination path for classification.
  const auto renameMarker = path.find(" => ");
  const auto realPath = (renameMarker != std::string::npos) ? path.substr(renameMarker + 4) : path;

  // Check project extension.
  std::filesystem::path p{realPath};
  if (!hasProjectExtension(p))
    return PathBucket::Other;

  // Check vendor.
  const auto baseName = p.filename().string();
  if (isVendoredFile(baseName, "") || pathHasVendoredDir(realPath))
    return PathBucket::Other;

  // Check test.
  if (pathHasTestDir(realPath) || isTestBasename(baseName))
    return PathBucket::Test;

  return PathBucket::Production;
}

// Aggregate churn by bucket (production vs test).
struct ChurnStats
{
  int prodAdded = 0;
  int prodRemoved = 0;
  int testAdded = 0;
  int testRemoved = 0;
  int prodFileCount = 0;
};

ChurnStats aggregateChurn(const std::vector<archcheck::git::NumStat> &numstat)
{
  ChurnStats stats;
  for (const auto &entry : numstat)
  {
    const auto bucket = classifyPath(entry.file);

    // Rename-only entries (added == 0 && removed == 0) are skipped.
    if (entry.added == 0 && entry.removed == 0)
      continue;

    if (bucket == PathBucket::Production)
    {
      stats.prodAdded += entry.added;
      stats.prodRemoved += entry.removed;
      ++stats.prodFileCount;
    }
    else if (bucket == PathBucket::Test)
    {
      stats.testAdded += entry.added;
      stats.testRemoved += entry.removed;
    }
  }
  return stats;
}

// Check thresholds: strict (prod >= 80, test == 0) or soft (prod >= 200, test < 5% of prod).
bool shouldReportCoEvolution(int prodChurn, int testChurn)
{
  if (prodChurn >= 80 && testChurn == 0)
    return true;
  if (prodChurn >= 200 && testChurn > 0 && testChurn < (prodChurn / 20))
    return true;
  return false;
}

} // namespace

rules::ViolationList detectTestCoEvolution(const std::vector<archcheck::git::NumStat> &numstat)
{
  rules::ViolationList result;
  const auto stats = aggregateChurn(numstat);

  const int prodChurn = stats.prodAdded + stats.prodRemoved;
  const int testChurn = stats.testAdded + stats.testRemoved;

  if (!shouldReportCoEvolution(prodChurn, testChurn))
    return result;

  std::ostringstream msg;
  msg << "prod +" << stats.prodAdded << "/-" << stats.prodRemoved << " across " << stats.prodFileCount
      << " file(s), tests +" << stats.testAdded << "/-" << stats.testRemoved;

  result.push_back({
      "TEST.1.prod_changed_tests_silent",
      "<diff>",
      0,
      msg.str(),
  });

  return result;
}

} // namespace archcheck::scan
