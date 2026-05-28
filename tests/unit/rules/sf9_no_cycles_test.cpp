#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/sf9_no_cycles.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::Sf9NoCycles;

TEST_CASE("SF.9: clean DAG has no violations", "[rules][sf9]")
{
  DependencyGraph g;
  const auto a = g.addNode("a.h");
  const auto b = g.addNode("b.h");
  const auto c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);

  Sf9NoCycles rule;
  REQUIRE(rule.check(g, {}).empty());
}

TEST_CASE("SF.9: 3-node cycle reports one violation with full chain", "[rules][sf9]")
{
  DependencyGraph g;
  const auto a = g.addNode("src/a.h");
  const auto b = g.addNode("src/b.h");
  const auto c = g.addNode("src/c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  g.addEdge(c, a);

  Sf9NoCycles rule;
  const auto v = rule.check(g, {});
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "SF.9");
  CHECK(v[0].line == 0);
  CHECK(v[0].message.find("cycle:") != std::string::npos);
  CHECK(v[0].message.find("src/a.h") != std::string::npos);
  CHECK(v[0].message.find("src/b.h") != std::string::npos);
  CHECK(v[0].message.find("src/c.h") != std::string::npos);
}

TEST_CASE("SF.9: two-node mutual cycle", "[rules][sf9]")
{
  DependencyGraph g;
  const auto a = g.addNode("a.h");
  const auto b = g.addNode("b.h");
  g.addEdge(a, b);
  g.addEdge(b, a);

  Sf9NoCycles rule;
  const auto v = rule.check(g, {});
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "SF.9");
}

TEST_CASE("SF.9: two separate cycles produce two violations", "[rules][sf9]")
{
  DependencyGraph g;
  const auto a = g.addNode("a.h");
  const auto b = g.addNode("b.h");
  const auto c = g.addNode("c.h");
  const auto d = g.addNode("d.h");
  g.addEdge(a, b);
  g.addEdge(b, a);
  g.addEdge(c, d);
  g.addEdge(d, c);

  Sf9NoCycles rule;
  REQUIRE(rule.check(g, {}).size() == 2);
}

TEST_CASE("SF.9: all-conditional cycle produces no violations", "[rules][sf9]")
{
  DependencyGraph g;
  const auto a = g.addNode("foo.h");
  const auto b = g.addNode("foo-inl.h");
  g.addEdge(a, b, /*conditional=*/true);
  g.addEdge(b, a, /*conditional=*/true);

  Sf9NoCycles rule;
  REQUIRE(rule.check(g, {}).empty());
}

TEST_CASE("SF.9: mixed cycle (one unconditional edge) still reports violation", "[rules][sf9]")
{
  DependencyGraph g;
  const auto a = g.addNode("a.h");
  const auto b = g.addNode("b.h");
  g.addEdge(a, b, /*conditional=*/true);
  g.addEdge(b, a, /*conditional=*/false);

  Sf9NoCycles rule;
  REQUIRE(rule.check(g, {}).size() == 1);
}
