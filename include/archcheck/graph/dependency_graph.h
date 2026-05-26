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
   NodeId add_node(std::string_view path);
   void add_edge(NodeId from, NodeId to);
   bool has_edge(NodeId from, NodeId to) const;
   const std::vector<NodeId>& successors(NodeId node) const;
   const std::vector<NodeId>& predecessors(NodeId node) const;
   std::size_t node_count() const;
   std::string_view path_of(NodeId node) const;

private:
   std::vector<std::string> paths_;
   std::unordered_map<std::string, NodeId> path_to_id_;
   std::unordered_map<NodeId, std::vector<NodeId>> forward_;
   std::unordered_map<NodeId, std::vector<NodeId>> reverse_;
};

} // namespace archcheck::graph
