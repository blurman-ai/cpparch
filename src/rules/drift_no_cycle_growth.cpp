#include "archcheck/rules/drift_no_cycle_growth.h"

#include <string>

#include "archcheck/graph/diff.h"

namespace archcheck::rules
{

DriftNoCycleGrowth::DriftNoCycleGrowth(graph::DependencyGraph baseline) : baseline_(std::move(baseline)) {}

ViolationList DriftNoCycleGrowth::check(const graph::DependencyGraph &graph,
                                        const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  ViolationList result;
  for (const auto &scc : graph::grownSccs(baseline_, graph))
  {
    std::string msg = scc.baselineSize == 0 ? "new cycle (" + std::to_string(scc.currentSize) + " members):"
                                            : "cycle grew from " + std::to_string(scc.baselineSize) + " to " +
                                                  std::to_string(scc.currentSize) + " members:";
    for (const auto &m : scc.members)
      msg += " " + std::string(graph.pathOf(m));
    result.push_back({"DRIFT.2", std::string(graph.pathOf(scc.members.front())), 0, msg});
  }
  return result;
}

} // namespace archcheck::rules
