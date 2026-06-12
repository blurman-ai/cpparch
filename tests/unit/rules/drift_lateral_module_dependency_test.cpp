#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "archcheck/graph/baseline.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/drift_lateral_module_dependency.h"
#include "archcheck/rules/violation.h"

using archcheck::graph::DependencyGraph;
using archcheck::graph::loadBaseline;
using archcheck::rules::DriftLateralModuleDependency;
using archcheck::rules::ViolationList;

namespace
{

// Build baseline from YAML string.
DependencyGraph loadYaml(const std::string &yaml)
{
  std::istringstream ss(yaml);
  auto [g, err] = loadBaseline(ss);
  if (err)
    throw std::runtime_error(err->message);
  return g;
}

auto noRead = [](std::string_view) { return std::string{}; };

int countId(const ViolationList &v, const std::string &id)
{
  int n = 0;
  for (const auto &vi : v)
    if (vi.ruleId == id)
      ++n;
  return n;
}

// Baseline: c→b, a→utils, b→utils, c→utils
// isShared(utils): FID=3, maxFID=3, FOD=0 ≤ medFOD=1 → shared
// isShared(b):     FID=1 < 0.5*3=1.5 → NOT shared
const std::string kBaseYaml = R"(format_version: "1"
nodes:
  - "a/a.h"
  - "b/b.h"
  - "c/c.h"
  - "utils/u.h"
edges:
  - [2, 1]
  - [0, 3]
  - [1, 3]
  - [2, 3]
)";

DependencyGraph makeCurrentWithNewEdge()
{
  DependencyGraph g;
  auto na = g.addNode("a/a.h");
  auto nb = g.addNode("b/b.h");
  auto nc = g.addNode("c/c.h");
  auto nu = g.addNode("utils/u.h");
  g.addEdge(na, nb); // NEW: a→b
  g.addEdge(na, nu); // baseline
  g.addEdge(nb, nu); // baseline
  g.addEdge(nc, nb); // baseline
  g.addEdge(nc, nu); // baseline
  return g;
}

} // namespace

TEST_CASE("DRIFT.4: first lateral pair fires DRIFT.4.NEW", "[rules][drift4]")
{
  DriftLateralModuleDependency rule(loadYaml(kBaseYaml));
  const auto v = rule.check(makeCurrentWithNewEdge(), noRead);
  REQUIRE(countId(v, "DRIFT.4.NEW") == 1);
  REQUIRE(countId(v, "DRIFT.4.CYCLE") == 0);
  REQUIRE(countId(v, "DRIFT.4.SDP") == 0);
}

TEST_CASE("DRIFT.4: no change — no violations", "[rules][drift4]")
{
  DriftLateralModuleDependency rule(loadYaml(kBaseYaml));
  // same as baseline
  DependencyGraph g;
  auto na = g.addNode("a/a.h");
  auto nb = g.addNode("b/b.h");
  auto nc = g.addNode("c/c.h");
  auto nu = g.addNode("utils/u.h");
  g.addEdge(na, nu);
  g.addEdge(nb, nu);
  g.addEdge(nc, nb);
  g.addEdge(nc, nu);
  REQUIRE(rule.check(g, noRead).empty());
}

TEST_CASE("DRIFT.4: new edge to shared layer — no violation", "[rules][drift4]")
{
  // a→utils is new (but utils is shared) → silent
  DependencyGraph baseline;
  auto nb2 = baseline.addNode("b/b.h");
  auto nc2 = baseline.addNode("c/c.h");
  auto nd2 = baseline.addNode("d/d.h");
  auto nu2 = baseline.addNode("utils/u.h");
  baseline.addEdge(nb2, nu2);
  baseline.addEdge(nc2, nu2);
  baseline.addEdge(nd2, nu2); // FID(utils)=3=maxFID → shared

  DependencyGraph current;
  auto na = current.addNode("a/a.h");
  auto nb = current.addNode("b/b.h");
  auto nc = current.addNode("c/c.h");
  auto nd = current.addNode("d/d.h");
  auto nu = current.addNode("utils/u.h");
  current.addEdge(nb, nu);
  current.addEdge(nc, nu);
  current.addEdge(nd, nu);
  current.addEdge(na, nu); // NEW: a→utils (shared → silent)

  DriftLateralModuleDependency rule(std::move(baseline));
  REQUIRE(rule.check(current, noRead).empty());
}

TEST_CASE("DRIFT.4: CYCLE fires when back-edge exists in baseline", "[rules][drift4]")
{
  // Baseline: b→a (back-edge), c→a, c→b
  DependencyGraph baseline;
  auto na = baseline.addNode("a/a.h");
  auto nb = baseline.addNode("b/b.h");
  auto nc = baseline.addNode("c/c.h");
  baseline.addEdge(nb, na); // b→a (back-edge for future CYCLE)
  baseline.addEdge(nc, na);
  baseline.addEdge(nc, nb);

  // Current: b→a, c→a, c→b + NEW a→b (creates cycle with b→a)
  DependencyGraph current;
  na = current.addNode("a/a.h");
  nb = current.addNode("b/b.h");
  nc = current.addNode("c/c.h");
  current.addEdge(nb, na);
  current.addEdge(nc, na);
  current.addEdge(nc, nb);
  current.addEdge(na, nb); // NEW: a→b → CYCLE

  DriftLateralModuleDependency rule(std::move(baseline));
  const auto v = rule.check(current, noRead);
  REQUIRE(countId(v, "DRIFT.4.CYCLE") == 1);
}

TEST_CASE("DRIFT.4: existing pair — no second violation", "[rules][drift4]")
{
  // baseline already has a→b → not new
  DependencyGraph baseline;
  auto na = baseline.addNode("a/a.h");
  auto nb = baseline.addNode("b/b.h");
  auto nc = baseline.addNode("c/c.h");
  baseline.addEdge(na, nb);
  baseline.addEdge(nc, nb);

  // current: a/z.h → b/b.h (second file-edge from same module pair)
  DependencyGraph current;
  na = current.addNode("a/a.h");
  auto naz = current.addNode("a/z.h");
  nb = current.addNode("b/b.h");
  nc = current.addNode("c/c.h");
  current.addEdge(na, nb);
  current.addEdge(naz, nb); // NEW file-edge, but pair (a,b) already exists
  current.addEdge(nc, nb);

  DriftLateralModuleDependency rule(std::move(baseline));
  REQUIRE(rule.check(current, noRead).empty());
}

TEST_CASE("DRIFT.4: mass-touch guard — silent when >150 added edges", "[rules][drift4]")
{
  DependencyGraph baseline;
  baseline.addNode("a/a.h");

  DependencyGraph current;
  // Add 151 new nodes and edges from a to all of them
  auto src = current.addNode("a/src.h");
  for (int i = 0; i < 151; ++i)
  {
    auto t = current.addNode("m" + std::to_string(i) + "/x.h");
    current.addEdge(src, t);
  }

  DriftLateralModuleDependency rule(std::move(baseline));
  REQUIRE(rule.check(current, noRead).empty()); // mass-touch guard
}
