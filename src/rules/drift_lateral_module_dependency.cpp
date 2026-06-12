#include "archcheck/rules/drift_lateral_module_dependency.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "archcheck/graph/diff.h"
#include "archcheck/graph/node_id.h"
#include "archcheck/rules/area_of.h"

namespace archcheck::rules
{

namespace
{

// From docs/research/lateral_module_drift_criterion.md §2.
// All counts are on DIRECT edges only (transitive would contaminate FID via
// include chains).
constexpr double kSharedFidRatio = 0.50;
constexpr double kSdpDelta = 0.10;
// Mass-touch guard: skip the whole rule when the diff is a reorg/vendor drop.
// Same threshold as the Python corpus scanner (§3 of the criterion doc).
constexpr int kMassTouchEdges = 150;

struct ModuleMetrics
{
  int fid = 0; // fan-in diversity: distinct modules that include into this one
  int fod = 0; // fan-out diversity: distinct modules this one includes
};

std::unordered_map<std::string, ModuleMetrics> computeMetrics(const graph::DependencyGraph &g)
{
  std::unordered_map<std::string, ModuleMetrics> m;
  // Use a set of pairs to avoid counting parallel file-edges between same modules.
  std::unordered_set<std::string> seen;
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(g.nodeCount()); ++i)
  {
    const graph::NodeId u{i};
    const std::string a = areaOf(g.pathOf(u));
    if (a.empty())
      continue;
    for (const auto v : g.successors(u))
    {
      const std::string b = areaOf(g.pathOf(v));
      if (b.empty() || a == b)
        continue;
      const std::string key = a + "\n" + b;
      if (!seen.insert(key).second)
        continue;
      m[a].fod++;
      m[b].fid++;
    }
  }
  return m;
}

// Martin instability I = Ce / (Ca + Ce), range [0,1].
// 0 = stable (many dependants, few dependencies); 1 = instable (leaf).
double instability(const ModuleMetrics &mm)
{
  const int total = mm.fid + mm.fod;
  return total > 0 ? static_cast<double>(mm.fod) / total : 0.5;
}

// MacCormack shared-layer heuristic: B is shared when many modules depend on it
// (FID >= 50% of max) and it depends on few itself (FOD <= median).
bool isShared(const std::string &mod, const std::unordered_map<std::string, ModuleMetrics> &metrics)
{
  if (metrics.empty())
    return false;
  int maxFid = 0;
  for (const auto &kv : metrics)
    if (kv.second.fid > maxFid)
      maxFid = kv.second.fid;
  if (maxFid == 0)
    return false;

  std::vector<int> fodVals;
  fodVals.reserve(metrics.size());
  for (const auto &kv : metrics)
    fodVals.push_back(kv.second.fod);
  std::sort(fodVals.begin(), fodVals.end());
  const int medFod = fodVals[fodVals.size() / 2];

  const auto it = metrics.find(mod);
  if (it == metrics.end())
    return false;
  // Strict > prevents integer truncation (e.g. 0.5*3=1.5→1) from misclassifying
  // modules with FID=1 as shared when the graph has few nodes.
  return it->second.fid > static_cast<int>(kSharedFidRatio * maxFid) && it->second.fod <= medFod;
}

// All module-level pairs in the graph (direct, not transitive).
std::unordered_set<std::string> modulePairs(const graph::DependencyGraph &g)
{
  std::unordered_set<std::string> pairs;
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
        pairs.insert(a + "\n" + b);
    }
  }
  return pairs;
}

// Assign grade string (CYCLE/SDP/NEW) for a new module-level dependency A→B.
std::string gradeEdge(const std::string &a, const std::string &b, const std::unordered_set<std::string> &basePairs,
                      const std::unordered_map<std::string, ModuleMetrics> &baseMetrics)
{
  if (basePairs.count(b + "\n" + a))
    return "DRIFT.4.CYCLE";
  const auto itA = baseMetrics.find(a);
  const auto itB = baseMetrics.find(b);
  const double iA = itA != baseMetrics.end() ? instability(itA->second) : 0.5;
  const double iB = itB != baseMetrics.end() ? instability(itB->second) : 0.5;
  return (iB > iA + kSdpDelta) ? "DRIFT.4.SDP" : "DRIFT.4.NEW";
}

// Build human-readable message for a graded violation.
std::string buildMsg(const std::string &ruleId, const std::string &a, const std::string &b, std::string_view fromPath,
                     std::string_view toPath)
{
  const std::string via = " (via " + std::string(fromPath) + " -> " + std::string(toPath) + ")";
  if (ruleId == "DRIFT.4.CYCLE")
    return "module '" + a + "' -> '" + b +
           "': lateral dependency creates module cycle"
           " ('" +
           b + "' already depends on '" + a + "' in baseline)" + via;
  if (ruleId == "DRIFT.4.SDP")
    return "module '" + a + "' -> '" + b + "': lateral dependency violates stability" + via;
  return "module '" + a + "' -> '" + b + "': first lateral dependency between peer modules" + via;
}

} // namespace

DriftLateralModuleDependency::DriftLateralModuleDependency(graph::DependencyGraph baseline)
    : baseline_(std::move(baseline))
{
}

ViolationList
DriftLateralModuleDependency::check(const graph::DependencyGraph &graph,
                                    const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  const auto added = graph::addedEdges(baseline_, graph);
  if (static_cast<int>(added.size()) > kMassTouchEdges)
    return {};
  const auto baseMetrics = computeMetrics(baseline_);
  const auto basePairs = modulePairs(baseline_);
  ViolationList result;
  std::unordered_set<std::string> reported;
  for (const auto &e : added)
  {
    const std::string a = areaOf(graph.pathOf(e.from));
    const std::string b = areaOf(graph.pathOf(e.to));
    if (a.empty() || b.empty() || a == b)
      continue;
    if (baseMetrics.find(b) == baseMetrics.end() || isShared(b, baseMetrics))
      continue;
    if (basePairs.count(a + "\n" + b) || !reported.insert(a + "\n" + b).second)
      continue;
    const std::string ruleId = gradeEdge(a, b, basePairs, baseMetrics);
    result.push_back({ruleId, std::string(graph.pathOf(e.from)), 0,
                      buildMsg(ruleId, a, b, graph.pathOf(e.from), graph.pathOf(e.to))});
  }
  return result;
}

} // namespace archcheck::rules
