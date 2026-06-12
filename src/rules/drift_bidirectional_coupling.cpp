#include "archcheck/rules/drift_bidirectional_coupling.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "archcheck/graph/diff.h"
#include "archcheck/graph/node_id.h"
#include "archcheck/rules/area_of.h"

namespace archcheck::rules
{

namespace
{

// Cross-area dependency pairs "A\nB" present in a graph (module A depends on B).
std::unordered_set<std::string> areaDeps(const graph::DependencyGraph &g)
{
  std::unordered_set<std::string> deps;
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(g.nodeCount()); ++i)
  {
    const graph::NodeId u{i};
    const std::string a = areaOf(g.pathOf(u));
    if (a.empty())
      continue;
    for (const auto v : g.successors(u))
    {
      const std::string b = areaOf(g.pathOf(v));
      if (!b.empty() && a != b)
        deps.insert(a + "\n" + b);
    }
  }
  return deps;
}

} // namespace

DriftBidirectionalCoupling::DriftBidirectionalCoupling(graph::DependencyGraph baseline) : baseline_(std::move(baseline))
{
}

ViolationList DriftBidirectionalCoupling::check(const graph::DependencyGraph &graph,
                                                const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  const auto baseDeps = areaDeps(baseline_);
  const auto curDeps = areaDeps(graph);
  ViolationList result;
  std::unordered_set<std::string> reported;
  for (const auto &e : graph::addedEdges(baseline_, graph))
  {
    const std::string a = areaOf(graph.pathOf(e.from));
    const std::string b = areaOf(graph.pathOf(e.to));
    if (a.empty() || b.empty() || a == b)
      continue;
    if (!curDeps.count(b + "\n" + a))
      continue; // not mutual now: B does not depend back on A
    if (baseDeps.count(a + "\n" + b) && baseDeps.count(b + "\n" + a))
      continue; // A<->B was already mutual in the baseline -> not new drift
    if (graph.hasEdge(e.to, e.from))
      continue; // direct two-file cycle is DRIFT.2 / SF.9 territory
    const std::string key = a < b ? a + "\n" + b : b + "\n" + a;
    if (!reported.insert(key).second)
      continue;
    result.push_back({"DRIFT.3", std::string(graph.pathOf(e.from)), 0,
                      "bidirectional module coupling: '" + a + "' <-> '" + b +
                          "' (modules now depend on each other; was not mutual in baseline)"});
  }
  return result;
}

} // namespace archcheck::rules
