#include "archcheck/graph/graph_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/file_classification.h"
#include "archcheck/scan/file_source.h"
#include "archcheck/scan/include_resolver.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::graph
{

namespace
{

struct FilterEntry
{
  scan::ProjectFile file;
  std::string content;
  bool hasBanner;
};

void applyResolved(const std::vector<scan::ResolvedInclude> &resolved, NodeId source, const std::vector<NodeId> &idMap,
                   DependencyGraph &dg, GraphBuildCounters &c)
{
  for (const auto &r : resolved)
  {
    switch (r.resolution)
    {
    case scan::Resolution::Project:
      dg.addEdge(source, idMap[r.target], r.directive.conditional);
      ++c.edges;
      break;
    case scan::Resolution::External:
      ++c.external;
      break;
    case scan::Resolution::Unresolved:
      ++c.unresolved;
      break;
    case scan::Resolution::Ambiguous:
      ++c.ambiguous;
      break;
    }
  }
}

// Vendored code (dirs #068, single-file libs #069) and unit/integration test
// code (#070) are not author architecture: vendored libs pollute cycles/metrics,
// and test code is not subject to architecture checks at all. Drop both before
// they enter the graph — this also stops tests/ from showing up as a cross-area
// dependency source. Reads each file once and caches the content for the include
// scan below.
struct FilteredFiles
{
  std::vector<scan::ProjectFile> files;
  std::vector<std::string> contents;
};

FilteredFiles filterVendored(scan::FileSource &source)
{
  std::vector<FilterEntry> candidates;
  for (const auto &f : source.list())
  {
    const std::string_view base = scan::baseName(f.path);
    if (scan::pathHasVendoredDir(f.path) || scan::pathHasTestDir(f.path) || scan::isTestBasename(base) ||
        scan::isVendoredBasename(base))
    {
      continue;
    }
    std::string src = source.read(f.path);
    const bool hasBanner = scan::hasVendorLicenseHeader(src);
    candidates.push_back({f, std::move(src), hasBanner});
  }

  // If >50% of project files carry a full license banner, it is the project's
  // own license (Apache/MIT/BSD), not a vendor signal — disable the banner layer.
  const std::size_t nBanner = static_cast<std::size_t>(
      std::count_if(candidates.begin(), candidates.end(), [](const FilterEntry &e) { return e.hasBanner; }));
  const bool dominant = !candidates.empty() && nBanner * 2 > candidates.size();

  FilteredFiles out;
  for (auto &e : candidates)
  {
    if (!dominant && e.hasBanner)
    {
      continue;
    }
    out.files.push_back(e.file);
    out.contents.push_back(std::move(e.content));
  }
  return out;
}

} // namespace

GraphBuildResult buildGraphForSource(scan::FileSource &source)
{
  GraphBuildResult result;
  const FilteredFiles kept = filterVendored(source);
  const auto &files = kept.files;
  const auto index = scan::buildProjectIndex(files);

  std::vector<NodeId> idMap;
  idMap.reserve(files.size());
  for (const auto &f : files)
  {
    idMap.push_back(result.graph.addNode(f.path));
  }

  for (std::size_t i = 0; i < files.size(); ++i)
  {
    const auto scanned = scan::scanIncludes(kept.contents[i]);
    result.counters.macro_includes += scanned.diagnostics.size();
    const auto resolved = scan::resolveIncludes(scanned.directives, files[i].path, files, index);
    applyResolved(resolved, idMap[i], idMap, result.graph, result.counters);
  }
  return result;
}

GraphBuildResult buildGraphForPath(const std::filesystem::path &root)
{
  scan::DiskFileSource source(root);
  return buildGraphForSource(source);
}

} // namespace archcheck::graph
