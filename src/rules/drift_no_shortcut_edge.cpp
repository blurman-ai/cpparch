#include "archcheck/rules/drift_no_shortcut_edge.h"

#include <cstdint>
#include <string>
#include <unordered_set>

#include "archcheck/graph/diff.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::rules
{

DriftNoShortcutEdge::DriftNoShortcutEdge(graph::DependencyGraph baseline) : baseline_(std::move(baseline)) {}

ViolationList DriftNoShortcutEdge::check(const graph::DependencyGraph &graph,
                                         const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  std::unordered_set<std::string> baselinePaths;
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(baseline_.nodeCount()); ++i)
    baselinePaths.emplace(baseline_.pathOf(graph::NodeId{i}));

  ViolationList result;
  for (const auto &e : graph::addedEdges(baseline_, graph))
  {
    const auto fromPath = std::string(graph.pathOf(e.from));
    const auto toPath = std::string(graph.pathOf(e.to));
    if (!baselinePaths.count(fromPath) || !baselinePaths.count(toPath))
      continue;
    result.push_back({"DRIFT.1", fromPath, 0, "shortcut edge: " + fromPath + " -> " + toPath});
  }
  return result;
}

} // namespace archcheck::rules
