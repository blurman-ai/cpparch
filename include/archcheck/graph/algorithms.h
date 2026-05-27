#pragma once

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

} // namespace archcheck::graph
