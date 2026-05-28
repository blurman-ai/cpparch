#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/lakos_god_headers.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::LakosGodHeaders;

TEST_CASE("Lakos.GodHeader: low fan-in — no violation", "[rules][lakos][god]")
{
  DependencyGraph g;
  const auto hub = g.addNode("base.h");
  const auto a = g.addNode("a.h");
  const auto b = g.addNode("b.h");
  g.addEdge(a, hub);
  g.addEdge(b, hub);

  LakosGodHeaders rule(5); // threshold=5, fan-in=2 → ok
  REQUIRE(rule.check(g, {}).empty());
}

TEST_CASE("Lakos.GodHeader: fan-in exceeds threshold → violation", "[rules][lakos][god]")
{
  DependencyGraph g;
  const auto hub = g.addNode("god.h");
  for (int i = 0; i < 6; ++i)
  {
    const auto n = g.addNode("x" + std::to_string(i) + ".h");
    g.addEdge(n, hub);
  }

  LakosGodHeaders rule(5); // threshold=5, fan-in=6 → violation
  const auto v = rule.check(g, {});
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "Lakos.GodHeader");
  CHECK(v[0].file == "god.h");
  CHECK(v[0].message.find("6") != std::string::npos);
}

TEST_CASE("Lakos.GodHeader: default threshold is 30", "[rules][lakos][god]")
{
  CHECK(LakosGodHeaders::kDefaultThreshold == 30);
}
