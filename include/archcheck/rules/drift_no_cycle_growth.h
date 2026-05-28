#pragma once

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// DRIFT.2: reports SCCs (include cycles) that are new or grew in membership
// relative to the baseline graph. A cycle growing from N to M members is a
// clear structural regression — each new member is trapped in a mutual
// dependency that prevents independent compilation and levelization.
// Suppression: update the graph baseline (--save-graph-baseline) after review.
class DriftNoCycleGrowth final : public IRule
{
public:
  explicit DriftNoCycleGrowth(graph::DependencyGraph baseline);
  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "DRIFT.2"; }

private:
  graph::DependencyGraph baseline_;
};

} // namespace archcheck::rules
