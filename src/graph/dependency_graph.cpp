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

const std::vector<NodeId> &empty_neighbors()
{
  static const std::vector<NodeId> empty;
  return empty;
}

bool contains(const std::vector<NodeId> &v, NodeId id) { return std::find(v.begin(), v.end(), id) != v.end(); }

std::uint64_t edge_key(NodeId from, NodeId to) { return (static_cast<std::uint64_t>(from.value) << 32) | to.value; }

} // namespace

NodeId DependencyGraph::addNode(std::string_view path)
{
  std::string normalized = normalize_path(path);
  auto it = pathToId_.find(normalized);
  if (it != pathToId_.end())
  {
    return it->second;
  }
  const NodeId id{static_cast<std::uint32_t>(paths_.size())};
  paths_.push_back(normalized);
  pathToId_.emplace(std::move(normalized), id);
  return id;
}

void DependencyGraph::addEdge(NodeId from, NodeId to, bool conditional)
{
  auto &fwd = forward_[from];
  if (contains(fwd, to))
  {
    if (!conditional)
      conditionalEdges_.erase(edge_key(from, to));
    return;
  }
  fwd.push_back(to);
  reverse_[to].push_back(from);
  if (conditional)
    conditionalEdges_.insert(edge_key(from, to));
}

bool DependencyGraph::isConditionalEdge(NodeId from, NodeId to) const
{
  return conditionalEdges_.count(edge_key(from, to)) > 0;
}

bool DependencyGraph::hasEdge(NodeId from, NodeId to) const
{
  auto it = forward_.find(from);
  if (it == forward_.end())
  {
    return false;
  }
  return contains(it->second, to);
}

const std::vector<NodeId> &DependencyGraph::successors(NodeId node) const
{
  auto it = forward_.find(node);
  return it == forward_.end() ? empty_neighbors() : it->second;
}

const std::vector<NodeId> &DependencyGraph::predecessors(NodeId node) const
{
  auto it = reverse_.find(node);
  return it == reverse_.end() ? empty_neighbors() : it->second;
}

std::size_t DependencyGraph::nodeCount() const { return paths_.size(); }

std::string_view DependencyGraph::pathOf(NodeId node) const { return paths_[node.value]; }

} // namespace archcheck::graph
