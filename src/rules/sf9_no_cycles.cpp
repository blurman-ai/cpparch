#include "archcheck/rules/sf9_no_cycles.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "archcheck/graph/algorithms.h"

namespace archcheck::rules
{

namespace
{

struct CycleFinder
{
  const graph::DependencyGraph &g;
  const std::unordered_set<graph::NodeId> &members;
  graph::NodeId target;
  std::unordered_set<graph::NodeId> vis;
  std::vector<graph::NodeId> path;

  bool dfs(graph::NodeId cur)
  {
    if (cur == target)
      return true;
    if (vis.count(cur))
      return false;
    vis.insert(cur);
    path.push_back(cur);
    for (graph::NodeId next : g.successors(cur))
    {
      if (!members.count(next))
        continue;
      if (dfs(next))
        return true;
    }
    path.pop_back();
    vis.erase(cur);
    return false;
  }
};

bool allEdgesConditional(const graph::DependencyGraph &g, graph::NodeId start, const std::vector<graph::NodeId> &path)
{
  if (path.empty())
    return false;
  if (!g.isConditionalEdge(start, path.front()))
    return false;
  for (std::size_t i = 0; i + 1 < path.size(); ++i)
  {
    if (!g.isConditionalEdge(path[i], path[i + 1]))
      return false;
  }
  return g.isConditionalEdge(path.back(), start);
}

std::string buildCycleMessage(const graph::DependencyGraph &g, const std::vector<graph::NodeId> &path,
                              graph::NodeId start)
{
  std::string s = "cycle: ";
  s += g.pathOf(start);
  for (const auto id : path)
  {
    s += " \xe2\x86\x92 "; // UTF-8 for →
    s += g.pathOf(id);
  }
  s += " \xe2\x86\x92 ";
  s += g.pathOf(start);
  return s;
}

} // namespace

ViolationList Sf9NoCycles::check(const graph::DependencyGraph &graph,
                                 const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  ViolationList result;
  for (const auto &scc : archcheck::graph::computeScc(graph))
  {
    if (scc.size() < 2)
      continue;
    const std::unordered_set<graph::NodeId> members(scc.begin(), scc.end());
    const graph::NodeId start = scc.front();
    CycleFinder finder{graph, members, start, {}, {}};
    for (graph::NodeId next : graph.successors(start))
    {
      if (members.count(next) && finder.dfs(next))
        break;
    }
    if (allEdgesConditional(graph, start, finder.path))
      continue;
    result.push_back({"SF.9", std::string(graph.pathOf(start)), 0, buildCycleMessage(graph, finder.path, start)});
  }
  return result;
}

} // namespace archcheck::rules
