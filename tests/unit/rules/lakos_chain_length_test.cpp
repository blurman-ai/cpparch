#include <catch2/catch_test_macros.hpp>

#include "archcheck/config/config.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/lakos_chain_length.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::LakosChainLength;

TEST_CASE("Lakos.ChainLength: short chain — no violation", "[rules][lakos][chain]")
{
  DependencyGraph g;
  const auto a = g.addNode("a.h");
  const auto b = g.addNode("b.h");
  g.addEdge(a, b);

  LakosChainLength rule(3); // threshold=3
  REQUIRE(rule.check(g, {}).empty());
}

TEST_CASE("Lakos.ChainLength: chain exceeding threshold → violations", "[rules][lakos][chain]")
{
  DependencyGraph g;
  // chain: n0 → n1 → n2 → n3 → n4 (depth of n0 = 4)
  std::vector<archcheck::graph::NodeId> nodes;
  for (int i = 0; i < 5; ++i)
    nodes.push_back(g.addNode("n" + std::to_string(i) + ".h"));
  for (int i = 0; i < 4; ++i)
    g.addEdge(nodes[i], nodes[i + 1]);

  LakosChainLength rule(3); // threshold=3, n0 depth=4 → violation
  const auto v = rule.check(g, {});
  REQUIRE(!v.empty());
  CHECK(v[0].ruleId == "Lakos.ChainLength");
  CHECK(v[0].message.find("4") != std::string::npos);
}

TEST_CASE("Lakos.ChainLength: empty graph — no violations", "[rules][lakos][chain]")
{
  DependencyGraph g;
  REQUIRE(LakosChainLength{}.check(g, {}).empty());
}

TEST_CASE("Lakos.ChainLength: default threshold is 10", "[rules][lakos][chain]")
{
  CHECK(archcheck::config::Thresholds{}.chainLength == 10);
}
