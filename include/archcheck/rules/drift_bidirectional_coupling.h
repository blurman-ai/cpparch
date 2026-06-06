#pragma once

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// DRIFT.3: reports a NEW module<->module mutual dependency at the AGGREGATE
// (area) level. Fires when, after the diff, modules A and B depend on each other
// (A->B and B->A at area level) while they were NOT mutually coupled in the
// baseline -- whether the reverse edge pre-existed and this diff closed the loop
// (incremental erosion) or both directions landed together (a module born
// coupled). Because the two directions go through *different* files, no file-level
// include cycle exists, so the modules just become non-levelizable (cannot be
// built or tested in isolation). Deliberately distinct from DRIFT.2 / SF.9, which
// own file-level cycles: a direct two-file A<->B include is skipped here so the
// rules never double-report the same edge.
//
// Area = first path component after stripping wrapper dirs (src/include/lib/..);
// build/vendor/test/generated dirs are ignored. Suppression: update the graph
// baseline (--save-graph-baseline) to accept the coupling.
class DriftBidirectionalCoupling final : public IRule
{
public:
  explicit DriftBidirectionalCoupling(graph::DependencyGraph baseline);
  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "DRIFT.3"; }

private:
  graph::DependencyGraph baseline_;
};

} // namespace archcheck::rules
