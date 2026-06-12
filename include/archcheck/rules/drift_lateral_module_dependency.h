#pragma once

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// DRIFT.4: detects the first lateral dependency between peer modules — a new
// include edge A→B where A and B are sibling modules that had no prior coupling.
//
// Graded by structural severity:
//   DRIFT.4.CYCLE — reverse edge B→A already exists in baseline (module cycle)
//   DRIFT.4.SDP   — I(B) > I(A) + 0.10; dependency goes against Martin's gradient
//   DRIFT.4.NEW   — first lateral pair; no structural issue yet, advisory
//
// B is considered a shared layer (skip) when FID(B) >= 0.5 * max_FID AND
// FOD(B) <= median_FOD, following MacCormack shared-component classification.
// Only DRIFT.4.CYCLE is gating; SDP and NEW are advisory.
//
// Research validation: 479 repos, TP 85%, CYCLE precision 92% (post-#117).
// See docs/research/lateral_module_drift_criterion.md for full criterion.
class DriftLateralModuleDependency final : public IRule
{
public:
  explicit DriftLateralModuleDependency(graph::DependencyGraph baseline);
  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "DRIFT.4"; }

private:
  graph::DependencyGraph baseline_;
};

} // namespace archcheck::rules
