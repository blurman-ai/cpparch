#pragma once

#include <unordered_set>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::graph
{

std::vector<std::vector<NodeId>> compute_scc(const DependencyGraph& g);
std::unordered_set<NodeId> reachable_from(const DependencyGraph& g, NodeId from);
std::unordered_set<NodeId> reverse_reachable_from(const DependencyGraph& g, NodeId from);
bool has_path(const DependencyGraph& g, NodeId from, NodeId to);

} // namespace archcheck::graph
