#include "archcheck/graph/graph_builder.h"

#include <string>
#include <vector>

#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/file_source.h"
#include "archcheck/scan/include_resolver.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::graph
{

namespace
{

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

} // namespace

GraphBuildResult buildGraphForSource(scan::FileSource &source)
{
  GraphBuildResult result;
  const auto files = source.list();
  const auto index = scan::buildProjectIndex(files);

  std::vector<NodeId> idMap;
  idMap.reserve(files.size());
  for (const auto &f : files)
  {
    idMap.push_back(result.graph.addNode(f.path));
  }

  for (std::size_t i = 0; i < files.size(); ++i)
  {
    const auto src = source.read(files[i].path);
    const auto scanned = scan::scanIncludes(src);
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
