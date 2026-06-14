#include "archcheck/rules/sf9_no_cycles.h"

#include <array>
#include <string>
#include <string_view>
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

bool endsWith(std::string_view s, std::string_view suf)
{
  return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// Markers of an inline/template implementation file that legitimately pairs with a
// same-stem header (foo.h + foo.inl / foo.ipp / foo.hxx / foo-inl.h / foo_impl.hpp ...).
// The _impl.* family is the dominant header-only convention (mlpack, Boost, Eigen):
// foo.hpp ends with #include "foo_impl.hpp", foo_impl.hpp includes foo.hpp back.
constexpr std::array<std::string_view, 14> kImplMarkers = {"-inl.h",    "_inl.h",   ".tmpl.h", ".impl.h",  ".inl",
                                                           ".ipp",      ".icc",     ".tcc",    ".tpp",     ".hxx",
                                                           "_impl.hpp", "_impl.hh", "_impl.h", "_impl.hxx"};

bool isImplName(std::string_view name)
{
  for (std::string_view m : kImplMarkers)
    if (endsWith(name, m))
      return true;
  return false;
}

std::string componentStem(std::string_view name)
{
  for (std::string_view m : kImplMarkers)
    if (endsWith(name, m))
      return std::string(name.substr(0, name.size() - m.size()));
  for (std::string_view ext : {".hpp", ".hh", ".h"})
    if (endsWith(name, ext))
      return std::string(name.substr(0, name.size() - ext.size()));
  return std::string(name);
}

std::string_view baseName(std::string_view path)
{
  const std::size_t s = path.rfind('/');
  return s == std::string_view::npos ? path : path.substr(s + 1);
}

std::string_view dirName(std::string_view path)
{
  const std::size_t s = path.rfind('/');
  return s == std::string_view::npos ? std::string_view{} : path.substr(0, s);
}

// foo.h <-> foo.inl (same dir + stem, one side an inline/template impl) is a single
// component split, not an architectural cycle: the include loop is broken by the
// guard. Only the strict 2-node form qualifies (#088).
bool isInlineSplitScc(const graph::DependencyGraph &g, const std::vector<graph::NodeId> &scc)
{
  if (scc.size() != 2)
    return false;
  const std::string pa(g.pathOf(scc[0]));
  const std::string pb(g.pathOf(scc[1]));
  if (dirName(pa) != dirName(pb) || componentStem(baseName(pa)) != componentStem(baseName(pb)))
    return false;
  return isImplName(baseName(pa)) || isImplName(baseName(pb));
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
    if (isInlineSplitScc(graph, scc))
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
