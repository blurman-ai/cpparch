#include "archcheck/rules/drift_bidirectional_coupling.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "archcheck/graph/diff.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::rules
{

namespace
{

const std::unordered_set<std::string> kWrapperDirs = {"src",      "source", "sources", "include",
                                                      "includes", "inc",    "lib",     "libs"};

const std::unordered_set<std::string> kNoiseDirs = {"build",
                                                    "_build",
                                                    "out",
                                                    "cmake-build-debug",
                                                    "cmake-build-release",
                                                    "vendor",
                                                    "third_party",
                                                    "thirdparty",
                                                    "external",
                                                    "externals",
                                                    "_deps",
                                                    "extern",
                                                    "test",
                                                    "tests",
                                                    "testing",
                                                    "examples",
                                                    "example",
                                                    "generated",
                                                    "node_modules",
                                                    "contrib",
                                                    "mock",
                                                    "mocks"};

std::string lower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Module name for a path: first path component after stripping wrapper dirs.
// Empty result = ignore this file (root-level file, or under a noise dir).
std::string areaOf(std::string_view path)
{
  std::vector<std::string> seg;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= path.size(); ++i)
  {
    if (i == path.size() || path[i] == '/')
    {
      if (i > start)
        seg.emplace_back(path.substr(start, i - start));
      start = i + 1;
    }
  }
  for (const auto &s : seg)
    if (kNoiseDirs.count(lower(s)))
      return {};
  std::size_t i = 0;
  while (i + 1 < seg.size() && kWrapperDirs.count(lower(seg[i])))
    ++i;
  if (i + 1 >= seg.size())
    return {}; // file sits directly under a wrapper or at root -> no module
  return seg[i];
}

// Cross-area dependency pairs "A\nB" present in a graph (module A depends on B).
std::unordered_set<std::string> areaDeps(const graph::DependencyGraph &g)
{
  std::unordered_set<std::string> deps;
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(g.nodeCount()); ++i)
  {
    const graph::NodeId u{i};
    const std::string a = areaOf(g.pathOf(u));
    if (a.empty())
      continue;
    for (const auto v : g.successors(u))
    {
      const std::string b = areaOf(g.pathOf(v));
      if (!b.empty() && a != b)
        deps.insert(a + "\n" + b);
    }
  }
  return deps;
}

} // namespace

DriftBidirectionalCoupling::DriftBidirectionalCoupling(graph::DependencyGraph baseline) : baseline_(std::move(baseline))
{
}

ViolationList DriftBidirectionalCoupling::check(const graph::DependencyGraph &graph,
                                                const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  const auto baseDeps = areaDeps(baseline_);
  const auto curDeps = areaDeps(graph);
  ViolationList result;
  std::unordered_set<std::string> reported;
  for (const auto &e : graph::addedEdges(baseline_, graph))
  {
    const std::string a = areaOf(graph.pathOf(e.from));
    const std::string b = areaOf(graph.pathOf(e.to));
    if (a.empty() || b.empty() || a == b)
      continue;
    if (!curDeps.count(b + "\n" + a))
      continue; // not mutual now: B does not depend back on A
    if (baseDeps.count(a + "\n" + b) && baseDeps.count(b + "\n" + a))
      continue; // A<->B was already mutual in the baseline -> not new drift
    if (graph.hasEdge(e.to, e.from))
      continue; // direct two-file cycle is DRIFT.2 / SF.9 territory
    const std::string key = a < b ? a + "\n" + b : b + "\n" + a;
    if (!reported.insert(key).second)
      continue;
    result.push_back({"DRIFT.3", std::string(graph.pathOf(e.from)), 0,
                      "bidirectional module coupling: '" + a + "' <-> '" + b +
                          "' (modules now depend on each other; was not mutual in baseline)"});
  }
  return result;
}

} // namespace archcheck::rules
