#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/drift_no_cycle_growth.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::DriftNoCycleGrowth;

TEST_CASE("DRIFT.2: no cycles in baseline or current — no violations", "[rules][drift2]")
{
  DependencyGraph baseline;
  const auto a = baseline.addNode("a.h");
  const auto b = baseline.addNode("b.h");
  baseline.addEdge(a, b);

  DependencyGraph current;
  const auto ca = current.addNode("a.h");
  const auto cb = current.addNode("b.h");
  current.addEdge(ca, cb);

  DriftNoCycleGrowth rule(baseline);
  REQUIRE(rule.check(current, {}).empty());
}

TEST_CASE("DRIFT.2: new 2-node cycle — violation", "[rules][drift2]")
{
  DependencyGraph baseline;
  baseline.addNode("a.h");
  baseline.addNode("b.h");
  // no cycle in baseline

  DependencyGraph current;
  const auto ca = current.addNode("a.h");
  const auto cb = current.addNode("b.h");
  current.addEdge(ca, cb);
  current.addEdge(cb, ca); // a <-> b: new cycle

  DriftNoCycleGrowth rule(baseline);
  const auto v = rule.check(current, {});
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "DRIFT.2");
  CHECK(v[0].line == 0);
  CHECK(v[0].message.find("new cycle") != std::string::npos);
  CHECK(v[0].message.find("a.h") != std::string::npos);
  CHECK(v[0].message.find("b.h") != std::string::npos);
}

TEST_CASE("DRIFT.2: existing cycle did not grow — no violation", "[rules][drift2]")
{
  DependencyGraph baseline;
  const auto a = baseline.addNode("a.h");
  const auto b = baseline.addNode("b.h");
  baseline.addEdge(a, b);
  baseline.addEdge(b, a); // cycle present in baseline

  DependencyGraph current;
  const auto ca = current.addNode("a.h");
  const auto cb = current.addNode("b.h");
  current.addEdge(ca, cb);
  current.addEdge(cb, ca); // same cycle — no regression

  DriftNoCycleGrowth rule(baseline);
  REQUIRE(rule.check(current, {}).empty());
}

TEST_CASE("DRIFT.2: 2-node cycle grew to 3 — violation", "[rules][drift2]")
{
  DependencyGraph baseline;
  const auto a = baseline.addNode("a.h");
  const auto b = baseline.addNode("b.h");
  baseline.addNode("c.h");
  baseline.addEdge(a, b);
  baseline.addEdge(b, a); // 2-node cycle in baseline

  DependencyGraph current;
  const auto ca = current.addNode("a.h");
  const auto cb = current.addNode("b.h");
  const auto cc = current.addNode("c.h");
  current.addEdge(ca, cb);
  current.addEdge(cb, cc);
  current.addEdge(cc, ca); // 3-node cycle

  DriftNoCycleGrowth rule(baseline);
  const auto v = rule.check(current, {});
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "DRIFT.2");
  CHECK(v[0].message.find("grew") != std::string::npos);
  CHECK(v[0].message.find("2") != std::string::npos);
  CHECK(v[0].message.find("3") != std::string::npos);
}

TEST_CASE("DRIFT.2: rule id", "[rules][drift2]")
{
  DriftNoCycleGrowth rule(DependencyGraph{});
  CHECK(rule.id() == "DRIFT.2");
}
