#pragma once

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// DRIFT.1: reports new include edges between two files that both existed in the
// baseline graph. An old file acquiring a new coupling to another old file is the
// classic AI-induced "shortcut" — locally convenient, globally architecture-eroding.
// Suppression: update the graph baseline (--save-graph-baseline) to accept the edge.
class DriftNoShortcutEdge final : public IRule
{
public:
  explicit DriftNoShortcutEdge(graph::DependencyGraph baseline);
  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "DRIFT.1"; }

private:
  graph::DependencyGraph baseline_;
};

} // namespace archcheck::rules
