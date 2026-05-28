#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/drift_no_shortcut_edge.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::DriftNoShortcutEdge;

TEST_CASE("DRIFT.1: no changes — no violations", "[rules][drift1]")
{
  DependencyGraph baseline;
  const auto a = baseline.addNode("a.h");
  const auto b = baseline.addNode("b.h");
  baseline.addEdge(a, b);

  DependencyGraph current;
  const auto ca = current.addNode("a.h");
  const auto cb = current.addNode("b.h");
  current.addEdge(ca, cb);

  DriftNoShortcutEdge rule(baseline);
  REQUIRE(rule.check(current, {}).empty());
}

TEST_CASE("DRIFT.1: new edge between two existing files — violation", "[rules][drift1]")
{
  DependencyGraph baseline;
  baseline.addNode("a.h");
  baseline.addNode("b.h");
  // no edge a -> b in baseline

  DependencyGraph current;
  const auto ca = current.addNode("a.h");
  const auto cb = current.addNode("b.h");
  current.addEdge(ca, cb); // shortcut: both existed, edge is new

  DriftNoShortcutEdge rule(baseline);
  const auto v = rule.check(current, {});
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "DRIFT.1");
  CHECK(v[0].file == "a.h");
  CHECK(v[0].line == 0);
  CHECK(v[0].message.find("a.h") != std::string::npos);
  CHECK(v[0].message.find("b.h") != std::string::npos);
}

TEST_CASE("DRIFT.1: new file adding include — no violation", "[rules][drift1]")
{
  DependencyGraph baseline;
  baseline.addNode("lib.h");
  // "new_feature.h" does not exist in baseline

  DependencyGraph current;
  const auto nf = current.addNode("new_feature.h");
  const auto lib = current.addNode("lib.h");
  current.addEdge(nf, lib); // new file -> old file: not a shortcut

  DriftNoShortcutEdge rule(baseline);
  REQUIRE(rule.check(current, {}).empty());
}

TEST_CASE("DRIFT.1: old file including new file — no violation", "[rules][drift1]")
{
  DependencyGraph baseline;
  baseline.addNode("api.h");
  // "detail.h" is new

  DependencyGraph current;
  const auto api = current.addNode("api.h");
  const auto detail = current.addNode("detail.h");
  current.addEdge(api, detail); // old -> new: toPath not in baseline

  DriftNoShortcutEdge rule(baseline);
  REQUIRE(rule.check(current, {}).empty());
}

TEST_CASE("DRIFT.1: empty baseline — no violations", "[rules][drift1]")
{
  DependencyGraph baseline;

  DependencyGraph current;
  const auto a = current.addNode("a.h");
  const auto b = current.addNode("b.h");
  current.addEdge(a, b);

  DriftNoShortcutEdge rule(baseline);
  REQUIRE(rule.check(current, {}).empty());
}

TEST_CASE("DRIFT.1: rule id", "[rules][drift1]")
{
  DriftNoShortcutEdge rule(DependencyGraph{});
  CHECK(rule.id() == "DRIFT.1");
}
