#include "archcheck/graph/algorithms.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_set>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::graph
{

namespace
{

constexpr std::uint32_t kUnvisited = std::numeric_limits<std::uint32_t>::max();

struct Frame
{
  NodeId node;
  std::size_t succ_index;
};

struct TarjanState
{
  std::vector<std::uint32_t> index_of;
  std::vector<std::uint32_t> lowlink_of;
  std::vector<bool> on_stack;
  std::vector<NodeId> component_stack;
  std::vector<Frame> call_stack;
  std::uint32_t next_index = 0;
  std::vector<std::vector<NodeId>> result;
};

void open_node(TarjanState &st, NodeId v)
{
  st.index_of[v.value] = st.next_index;
  st.lowlink_of[v.value] = st.next_index;
  ++st.next_index;
  st.component_stack.push_back(v);
  st.on_stack[v.value] = true;
}

void emit_component(TarjanState &st, NodeId v)
{
  std::vector<NodeId> comp;
  while (true)
  {
    const NodeId w = st.component_stack.back();
    st.component_stack.pop_back();
    st.on_stack[w.value] = false;
    comp.push_back(w);
    if (w == v)
    {
      break;
    }
  }
  st.result.push_back(std::move(comp));
}

void close_node(TarjanState &st, NodeId v)
{
  if (st.lowlink_of[v.value] == st.index_of[v.value])
  {
    emit_component(st, v);
  }
  st.call_stack.pop_back();
  if (!st.call_stack.empty())
  {
    const NodeId parent = st.call_stack.back().node;
    st.lowlink_of[parent.value] = std::min(st.lowlink_of[parent.value], st.lowlink_of[v.value]);
  }
}

bool step_into_successor(const DependencyGraph &g, TarjanState &st, Frame &frame)
{
  const auto &succ = g.successors(frame.node);
  while (frame.succ_index < succ.size())
  {
    const NodeId w = succ[frame.succ_index++];
    if (st.index_of[w.value] == kUnvisited)
    {
      st.call_stack.push_back(Frame{w, 0});
      return true;
    }
    if (st.on_stack[w.value])
    {
      st.lowlink_of[frame.node.value] = std::min(st.lowlink_of[frame.node.value], st.index_of[w.value]);
    }
  }
  return false;
}

void run_tarjan_from(const DependencyGraph &g, TarjanState &st, NodeId root)
{
  st.call_stack.push_back(Frame{root, 0});
  open_node(st, root);
  while (!st.call_stack.empty())
  {
    Frame &frame = st.call_stack.back();
    if (step_into_successor(g, st, frame))
    {
      open_node(st, st.call_stack.back().node);
      continue;
    }
    close_node(st, frame.node);
  }
}

void sort_components(std::vector<std::vector<NodeId>> &sccs)
{
  for (auto &c : sccs)
  {
    std::sort(c.begin(), c.end(), [](NodeId a, NodeId b) { return a.value < b.value; });
  }
  std::sort(sccs.begin(), sccs.end(), [](const auto &a, const auto &b) { return a.front().value < b.front().value; });
}

std::unordered_set<NodeId> bfs(const DependencyGraph &g, NodeId start, bool forward)
{
  std::unordered_set<NodeId> visited;
  std::queue<NodeId> q;
  visited.insert(start);
  q.push(start);
  while (!q.empty())
  {
    const NodeId v = q.front();
    q.pop();
    const auto &neighbors = forward ? g.successors(v) : g.predecessors(v);
    for (NodeId n : neighbors)
    {
      if (visited.insert(n).second)
      {
        q.push(n);
      }
    }
  }
  return visited;
}

} // namespace

std::vector<std::vector<NodeId>> computeScc(const DependencyGraph &g)
{
  const std::size_t n = g.nodeCount();
  TarjanState st;
  st.index_of.assign(n, kUnvisited);
  st.lowlink_of.assign(n, 0);
  st.on_stack.assign(n, false);
  for (std::uint32_t i = 0; i < n; ++i)
  {
    if (st.index_of[i] == kUnvisited)
    {
      run_tarjan_from(g, st, NodeId{i});
    }
  }
  sort_components(st.result);
  return std::move(st.result);
}

std::unordered_set<NodeId> reachableFrom(const DependencyGraph &g, NodeId from) { return bfs(g, from, true); }

std::unordered_set<NodeId> reverseReachableFrom(const DependencyGraph &g, NodeId from) { return bfs(g, from, false); }

bool hasPath(const DependencyGraph &g, NodeId from, NodeId to)
{
  if (from == to)
  {
    return true;
  }
  std::unordered_set<NodeId> visited;
  std::queue<NodeId> q;
  visited.insert(from);
  q.push(from);
  while (!q.empty())
  {
    const NodeId v = q.front();
    q.pop();
    for (NodeId n : g.successors(v))
    {
      if (n == to)
      {
        return true;
      }
      if (visited.insert(n).second)
      {
        q.push(n);
      }
    }
  }
  return false;
}

} // namespace archcheck::graph
