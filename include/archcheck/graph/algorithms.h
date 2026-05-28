#pragma once

#include <cstddef>
#include <unordered_set>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::graph
{

std::vector<std::vector<NodeId>> computeScc(const DependencyGraph &g);
std::unordered_set<NodeId> reachableFrom(const DependencyGraph &g, NodeId from);
std::unordered_set<NodeId> reverseReachableFrom(const DependencyGraph &g, NodeId from);
bool hasPath(const DependencyGraph &g, NodeId from, NodeId to);

// Returns the longest include chain depth for each node (indexed by NodeId.value).
// Depth = longest path from this node to any leaf. Cycles are condensed first.
std::vector<std::size_t> computeIncludeDepths(const DependencyGraph &g);

// Returns incoming edge count for each node (indexed by NodeId.value).
std::vector<std::size_t> computeFanIn(const DependencyGraph &g);

struct GraphMetrics
{
  std::size_t nodeCount = 0;
  std::size_t maxChainLength = 0;
  std::size_t maxFanIn = 0;
  std::size_t ccd = 0; // Cumulative Component Dependency = sum of CD(x) for all x
  double acd = 0.0;    // Average CD = CCD / N
  double nccd = 0.0;   // Normalized ACD = ACD / log2(N+1)
};

GraphMetrics computeGraphMetrics(const DependencyGraph &g);

} // namespace archcheck::graph
