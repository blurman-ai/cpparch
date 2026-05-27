#pragma once

#include <cstddef>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::graph
{

struct EdgeRef
{
  NodeId from;
  NodeId to;
};

// Edges present in `current` but not in `baseline`. Both graphs are
// compared by path (string), not by NodeId — NodeIds are session-local.
// Returned EdgeRef uses NodeIds from the `current` graph.
std::vector<EdgeRef> addedEdges(const DependencyGraph &baseline, const DependencyGraph &current);

// Edges present in `baseline` but not in `current`. Returned EdgeRef uses
// NodeIds from the `current` graph; if an endpoint exists only in baseline,
// it is materialised as a fresh node in a temporary view — instead we skip
// such edges, because a removed edge whose endpoint also disappeared is
// redundant for drift reporting.
std::vector<EdgeRef> removedEdges(const DependencyGraph &baseline, const DependencyGraph &current);

struct GrownScc
{
  std::vector<NodeId> members; // NodeIds from `current` graph
  std::size_t baseline_size;
  std::size_t current_size;
};

// SCCs in `current` of size >= 2 that are larger than (or new compared to)
// the matching SCC in baseline. Matching is by member-path overlap: a current
// SCC matches the baseline SCC with which it shares the most paths. If the
// matched baseline SCC is strictly smaller (or none exists), the current SCC
// is reported with the corresponding baseline_size (0 if no match).
std::vector<GrownScc> grownSccs(const DependencyGraph &baseline, const DependencyGraph &current);

} // namespace archcheck::graph
