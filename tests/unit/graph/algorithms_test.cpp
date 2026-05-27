#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

using archcheck::graph::computeScc;
using archcheck::graph::DependencyGraph;
using archcheck::graph::hasPath;
using archcheck::graph::NodeId;
using archcheck::graph::reachableFrom;
using archcheck::graph::reverseReachableFrom;

TEST_CASE("compute_scc on empty graph returns empty result", "[graph][algorithms][scc]")
{
  DependencyGraph g;
  REQUIRE(computeScc(g).empty());
}

TEST_CASE("compute_scc on DAG yields singleton SCCs", "[graph][algorithms][scc]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  const auto sccs = computeScc(g);
  REQUIRE(sccs.size() == 3);
  for (const auto &s : sccs)
  {
    REQUIRE(s.size() == 1);
  }
}

TEST_CASE("compute_scc detects a single 3-node cycle", "[graph][algorithms][scc]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  g.addEdge(c, a);
  const auto sccs = computeScc(g);
  REQUIRE(sccs.size() == 1);
  REQUIRE(sccs[0].size() == 3);
}

TEST_CASE("compute_scc separates two independent components", "[graph][algorithms][scc]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  const NodeId d = g.addNode("d.h");
  g.addEdge(a, b);
  g.addEdge(b, a);
  g.addEdge(c, d);
  const auto sccs = computeScc(g);
  REQUIRE(sccs.size() == 3);
  // Two singletons (c, d) and one 2-cycle (a, b). Ordering is by smallest NodeId in each.
  REQUIRE(sccs[0].size() == 2);
  REQUIRE(sccs[0].front() == a);
  REQUIRE(sccs[1].size() == 1);
  REQUIRE(sccs[1].front() == c);
  REQUIRE(sccs[2].size() == 1);
  REQUIRE(sccs[2].front() == d);
}

TEST_CASE("compute_scc output is deterministic across runs", "[graph][algorithms][scc]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  g.addEdge(c, a);
  const auto first = computeScc(g);
  const auto second = computeScc(g);
  REQUIRE(first == second);
}

TEST_CASE("reachable_from walks forward edges", "[graph][algorithms][reachability]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  const NodeId d = g.addNode("d.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  const auto r = reachableFrom(g, a);
  REQUIRE(r.size() == 3);
  REQUIRE(r.count(a) == 1);
  REQUIRE(r.count(b) == 1);
  REQUIRE(r.count(c) == 1);
  REQUIRE(r.count(d) == 0);
}

TEST_CASE("reachable_from on isolated node returns just itself", "[graph][algorithms][reachability]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const auto r = reachableFrom(g, a);
  REQUIRE(r.size() == 1);
  REQUIRE(r.count(a) == 1);
}

TEST_CASE("reverseReachableFrom mirrors forward reachability", "[graph][algorithms][reachability]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  const auto fwd = reachableFrom(g, a);
  const auto rev = reverseReachableFrom(g, c);
  REQUIRE(fwd == rev);
}

TEST_CASE("hasPath returns true for direct edge", "[graph][algorithms][path]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  g.addEdge(a, b);
  REQUIRE(hasPath(g, a, b));
}

TEST_CASE("hasPath returns true for transitive reachability", "[graph][algorithms][path]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(b, c);
  REQUIRE(hasPath(g, a, c));
}

TEST_CASE("hasPath returns false for unreachable target", "[graph][algorithms][path]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  REQUIRE_FALSE(hasPath(g, a, b));
}

TEST_CASE("hasPath returns true for self-path", "[graph][algorithms][path]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  REQUIRE(hasPath(g, a, a));
}

TEST_CASE("hasPath respects edge direction", "[graph][algorithms][path]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  g.addEdge(a, b);
  REQUIRE_FALSE(hasPath(g, b, a));
}
