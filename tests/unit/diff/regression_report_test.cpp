#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "archcheck/diff/regression_report.h"
#include "archcheck/graph/dependency_graph.h"

using archcheck::diff::buildRegressionReport;
using archcheck::diff::MetricThresholds;
using archcheck::diff::writeTextReport;
using archcheck::graph::DependencyGraph;
using archcheck::graph::NodeId;

namespace
{

DependencyGraph chain_abc()
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  return g;
}

} // namespace

TEST_CASE("buildRegressionReport: no diff → empty, no regression", "[diff][report]")
{
  const auto baseline = chain_abc();
  const auto current = chain_abc();
  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.addedEdges.empty());
  REQUIRE(r.removedEdges.empty());
  REQUIRE(r.grownCycles.empty());
  REQUIRE_FALSE(r.hasRegression());
}

TEST_CASE("buildRegressionReport: shortcut edge a->c is reported as added", "[diff][report]")
{
  const auto baseline = chain_abc();
  DependencyGraph current = chain_abc();
  current.addEdge(NodeId{0}, NodeId{2});

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.addedEdges.size() == 1);
  REQUIRE(r.addedEdges[0].from == "a.h");
  REQUIRE(r.addedEdges[0].to == "c.h");
  REQUIRE(r.hasRegression());
  REQUIRE_FALSE(r.gates()); // added edge alone is advisory, must not fail CI
}

TEST_CASE("buildRegressionReport: new cycle is reported as grown SCC", "[diff][report]")
{
  const auto baseline = chain_abc();
  DependencyGraph current = chain_abc();
  current.addEdge(NodeId{2}, NodeId{0}); // closes a->b->c->a

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE_FALSE(r.grownCycles.empty());
  REQUIRE(r.grownCycles[0].currentSize == 3);
  REQUIRE(r.grownCycles[0].baselineSize == 0);
  REQUIRE(r.hasRegression());
  REQUIRE(r.gates());
}

TEST_CASE("buildRegressionReport: first cross-area dependency is reported once per area pair", "[diff][report]")
{
  DependencyGraph baseline;
  const auto srcA = baseline.addNode("src/a.h");
  const auto srcB = baseline.addNode("src/b.h");
  baseline.addEdge(srcA, srcB);

  DependencyGraph current = baseline;
  const auto appA = current.addNode("app/a.h");
  const auto appB = current.addNode("app/b.h");
  current.addEdge(appA, NodeId{0});
  current.addEdge(appB, NodeId{1});

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.newCrossAreaDependencies.size() == 1);
  REQUIRE(r.newCrossAreaDependencies[0].fromArea == "app");
  REQUIRE(r.newCrossAreaDependencies[0].toArea == "src");
  REQUIRE(r.newCrossAreaDependencies[0].edgeCount == 2);
  REQUIRE(r.hasRegression());
}

TEST_CASE("buildRegressionReport: existing cross-area dependency growth is not reported as new", "[diff][report]")
{
  DependencyGraph baseline;
  const auto srcA = baseline.addNode("src/a.h");
  const auto srcB = baseline.addNode("src/b.h");
  const auto testA = baseline.addNode("tests/test_a.h");
  baseline.addEdge(testA, srcA);

  DependencyGraph current = baseline;
  const auto testB = current.addNode("tests/test_b.h");
  current.addEdge(testB, srcB);

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.newCrossAreaDependencies.empty());
}

TEST_CASE("writeTextReport: removed-only diff is not a regression", "[diff][report]")
{
  // Removing edges should NOT trip exit-code-1 in CI — PRs that drop edges
  // make architecture stricter, not worse.
  DependencyGraph baseline = chain_abc();
  DependencyGraph current;
  current.addNode("a.h");
  current.addNode("b.h");
  current.addNode("c.h");
  // current has no edges at all.

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.addedEdges.empty());
  REQUIRE(r.removedEdges.size() == 2);
  REQUIRE_FALSE(r.hasRegression());

  std::ostringstream out;
  writeTextReport(r, out);
  const auto s = out.str();
  REQUIRE(s.find("added_edges:    0") != std::string::npos);
  REQUIRE(s.find("removed_edges:  2") != std::string::npos);
}

TEST_CASE("writeTextReport: added edge appears in output", "[diff][report]")
{
  const auto baseline = chain_abc();
  DependencyGraph current = chain_abc();
  current.addEdge(NodeId{0}, NodeId{2}); // a->c shortcut

  const auto r = buildRegressionReport(baseline, current);

  std::ostringstream out;
  writeTextReport(r, out);
  const auto s = out.str();
  REQUIRE(s.find("added_edges:    1") != std::string::npos);
  REQUIRE(s.find("added (advisory):") != std::string::npos);
  REQUIRE(s.find("gate: ok") != std::string::npos);
  REQUIRE(s.find("a.h") != std::string::npos);
  REQUIRE(s.find("c.h") != std::string::npos);
}

TEST_CASE("writeTextReport: grown cycle appears in output", "[diff][report]")
{
  const auto baseline = chain_abc();
  DependencyGraph current = chain_abc();
  current.addEdge(NodeId{2}, NodeId{0}); // c->a closes a cycle

  const auto r = buildRegressionReport(baseline, current);

  std::ostringstream out;
  writeTextReport(r, out);
  const auto s = out.str();
  REQUIRE(s.find("grown_cycles:   1") != std::string::npos);
  REQUIRE(s.find("grown_cycles (gating):") != std::string::npos);
  REQUIRE(s.find("size") != std::string::npos);
  REQUIRE(s.find("gate: fail") != std::string::npos);
}

TEST_CASE("writeTextReport: empty report shows zeros only", "[diff][report]")
{
  const auto baseline = chain_abc();
  const auto current = chain_abc();
  const auto r = buildRegressionReport(baseline, current);

  std::ostringstream out;
  writeTextReport(r, out);
  const auto s = out.str();
  REQUIRE(s.find("added_edges:    0") != std::string::npos);
  REQUIRE(s.find("removed_edges:  0") != std::string::npos);
  REQUIRE(s.find("grown_cycles:   0") != std::string::npos);
  REQUIRE(s.find("new_area_deps:  0") != std::string::npos);
  // No added:/removed:/grown_cycles: sections when empty
  REQUIRE(s.find("\nadded:\n") == std::string::npos);
  REQUIRE(s.find("\nremoved:\n") == std::string::npos);
  REQUIRE(s.find("\ngrown_cycles:\n") == std::string::npos);
}

TEST_CASE("writeTextReport: new cross-area dependency appears in output", "[diff][report]")
{
  DependencyGraph baseline;
  const auto srcA = baseline.addNode("src/a.h");
  const auto srcB = baseline.addNode("src/b.h");
  baseline.addEdge(srcA, srcB);

  DependencyGraph current = baseline;
  const auto testA = current.addNode("tests/test_a.h");
  current.addEdge(testA, NodeId{0});

  const auto r = buildRegressionReport(baseline, current);

  std::ostringstream out;
  writeTextReport(r, out);
  const auto s = out.str();
  REQUIRE(s.find("new_area_deps:  1") != std::string::npos);
  REQUIRE(s.find("new_cross_area_dependencies (advisory):") != std::string::npos);
  REQUIRE(s.find("tests -> src") != std::string::npos);
  REQUIRE(s.find("tests/test_a.h") != std::string::npos);
}

// --- metric regression tests ---

TEST_CASE("buildRegressionReport: chain length grows → chainLengthGrown set", "[diff][report][metrics]")
{
  // baseline: a->b (depth 1), current: a->b->c->d (depth 3)
  DependencyGraph baseline;
  const auto a0 = baseline.addNode("a.h");
  const auto b0 = baseline.addNode("b.h");
  baseline.addEdge(a0, b0);

  DependencyGraph current;
  const auto a1 = current.addNode("a.h");
  const auto b1 = current.addNode("b.h");
  const auto c1 = current.addNode("c.h");
  const auto d1 = current.addNode("d.h");
  current.addEdge(a1, b1);
  current.addEdge(b1, c1);
  current.addEdge(c1, d1);

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.chainLengthGrown.has_value());
  REQUIRE(r.chainLengthGrown->baseline == 1);
  REQUIRE(r.chainLengthGrown->current == 3);
  REQUIRE(r.hasRegression());
  REQUIRE_FALSE(r.gates());
}

TEST_CASE("buildRegressionReport: chain length unchanged → chainLengthGrown nullopt", "[diff][report][metrics]")
{
  const auto r = buildRegressionReport(chain_abc(), chain_abc());
  REQUIRE_FALSE(r.chainLengthGrown.has_value());
}

TEST_CASE("buildRegressionReport: new god-header → newGodHeaders non-empty", "[diff][report][metrics]")
{
  // threshold = 3; PR adds 4 files all including hub.h → hub.h becomes god-header
  MetricThresholds t;
  t.godHeaderFanIn = 3;

  DependencyGraph baseline;
  const auto hub0 = baseline.addNode("hub.h");
  for (int i = 0; i < 3; ++i)
  {
    auto n = baseline.addNode("c" + std::to_string(i) + ".h");
    baseline.addEdge(n, hub0);
  }

  DependencyGraph current;
  const auto hub1 = current.addNode("hub.h");
  for (int i = 0; i < 4; ++i)
  {
    auto n = current.addNode("c" + std::to_string(i) + ".h");
    current.addEdge(n, hub1);
  }

  const auto r = buildRegressionReport(baseline, current, t);
  REQUIRE(r.newGodHeaders.size() == 1);
  REQUIRE(r.newGodHeaders[0] == "hub.h");
  REQUIRE(r.hasRegression());
  REQUIRE(r.gates());
}

TEST_CASE("buildRegressionReport: NCCD grows → nccdDelta set positive", "[diff][report][metrics]")
{
  // baseline: a, b, c isolated (NCCD low); current: fully connected star a->b, a->c, b->c
  DependencyGraph baseline;
  baseline.addNode("a.h");
  baseline.addNode("b.h");
  baseline.addNode("c.h");

  DependencyGraph current;
  const auto a = current.addNode("a.h");
  const auto b = current.addNode("b.h");
  const auto c = current.addNode("c.h");
  current.addEdge(a, b);
  current.addEdge(a, c);
  current.addEdge(b, c);

  const auto r = buildRegressionReport(baseline, current);
  REQUIRE(r.nccdDelta.has_value());
  REQUIRE(*r.nccdDelta > 0.0);
  REQUIRE(r.hasRegression());
  REQUIRE_FALSE(r.gates());
}

TEST_CASE("writeTextReport: metric regressions appear in output", "[diff][report][metrics]")
{
  DependencyGraph baseline;
  const auto a0 = baseline.addNode("a.h");
  const auto b0 = baseline.addNode("b.h");
  baseline.addEdge(a0, b0);

  DependencyGraph current;
  const auto a1 = current.addNode("a.h");
  const auto b1 = current.addNode("b.h");
  const auto c1 = current.addNode("c.h");
  current.addEdge(a1, b1);
  current.addEdge(b1, c1);

  MetricThresholds t;
  t.godHeaderFanIn = 0; // every node with fan-in > 0 is a god-header
  const auto r = buildRegressionReport(baseline, current, t);

  std::ostringstream out;
  writeTextReport(r, out);
  const auto s = out.str();
  REQUIRE(s.find("chain_length_grown (advisory):") != std::string::npos);
  REQUIRE(s.find("new_god_headers (gating):") != std::string::npos);
  REQUIRE(s.find("nccd_grown (advisory):") != std::string::npos);
}
