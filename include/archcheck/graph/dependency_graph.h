#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "archcheck/graph/node_id.h"

namespace archcheck::graph
{

class DependencyGraph
{
public:
  NodeId addNode(std::string_view path);
  void addEdge(NodeId from, NodeId to);
  bool hasEdge(NodeId from, NodeId to) const;
  const std::vector<NodeId> &successors(NodeId node) const;
  const std::vector<NodeId> &predecessors(NodeId node) const;
  std::size_t nodeCount() const;
  std::string_view pathOf(NodeId node) const;

private:
  std::vector<std::string> paths_;
  std::unordered_map<std::string, NodeId> path_to_id_;
  std::unordered_map<NodeId, std::vector<NodeId>> forward_;
  std::unordered_map<NodeId, std::vector<NodeId>> reverse_;
};

} // namespace archcheck::graph
