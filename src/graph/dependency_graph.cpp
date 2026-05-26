#include "archcheck/graph/dependency_graph.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace archcheck::graph
{

namespace
{

std::string normalize_path(std::string_view path)
{
   std::string s;
   s.reserve(path.size());
   for (char c : path)
   {
      s.push_back(c == '\\' ? '/' : c);
   }
   std::size_t i = 0;
   while (i + 1 < s.size() && s[i] == '.' && s[i + 1] == '/')
   {
      i += 2;
   }
   return s.substr(i);
}

const std::vector<NodeId>& empty_neighbors()
{
   static const std::vector<NodeId> empty;
   return empty;
}

bool contains(const std::vector<NodeId>& v, NodeId id)
{
   return std::find(v.begin(), v.end(), id) != v.end();
}

} // namespace

NodeId DependencyGraph::add_node(std::string_view path)
{
   std::string normalized = normalize_path(path);
   auto it = path_to_id_.find(normalized);
   if (it != path_to_id_.end())
   {
      return it->second;
   }
   const NodeId id{static_cast<std::uint32_t>(paths_.size())};
   paths_.push_back(normalized);
   path_to_id_.emplace(std::move(normalized), id);
   return id;
}

void DependencyGraph::add_edge(NodeId from, NodeId to)
{
   auto& fwd = forward_[from];
   if (contains(fwd, to))
   {
      return;
   }
   fwd.push_back(to);
   reverse_[to].push_back(from);
}

bool DependencyGraph::has_edge(NodeId from, NodeId to) const
{
   auto it = forward_.find(from);
   if (it == forward_.end())
   {
      return false;
   }
   return contains(it->second, to);
}

const std::vector<NodeId>& DependencyGraph::successors(NodeId node) const
{
   auto it = forward_.find(node);
   return it == forward_.end() ? empty_neighbors() : it->second;
}

const std::vector<NodeId>& DependencyGraph::predecessors(NodeId node) const
{
   auto it = reverse_.find(node);
   return it == reverse_.end() ? empty_neighbors() : it->second;
}

std::size_t DependencyGraph::node_count() const
{
   return paths_.size();
}

std::string_view DependencyGraph::path_of(NodeId node) const
{
   return paths_[node.value];
}

} // namespace archcheck::graph
