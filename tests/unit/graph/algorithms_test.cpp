#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

using archcheck::graph::compute_scc;
using archcheck::graph::DependencyGraph;
using archcheck::graph::has_path;
using archcheck::graph::NodeId;
using archcheck::graph::reachable_from;
using archcheck::graph::reverse_reachable_from;

TEST_CASE("compute_scc on empty graph returns empty result", "[graph][algorithms][scc]")
{
   DependencyGraph g;
   REQUIRE(compute_scc(g).empty());
}

TEST_CASE("compute_scc on DAG yields singleton SCCs", "[graph][algorithms][scc]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, b);
   g.add_edge(b, c);
   const auto sccs = compute_scc(g);
   REQUIRE(sccs.size() == 3);
   for (const auto& s : sccs)
   {
      REQUIRE(s.size() == 1);
   }
}

TEST_CASE("compute_scc detects a single 3-node cycle", "[graph][algorithms][scc]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, b);
   g.add_edge(b, c);
   g.add_edge(c, a);
   const auto sccs = compute_scc(g);
   REQUIRE(sccs.size() == 1);
   REQUIRE(sccs[0].size() == 3);
}

TEST_CASE("compute_scc separates two independent components", "[graph][algorithms][scc]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   const NodeId d = g.add_node("d.h");
   g.add_edge(a, b);
   g.add_edge(b, a);
   g.add_edge(c, d);
   const auto sccs = compute_scc(g);
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
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, b);
   g.add_edge(b, c);
   g.add_edge(c, a);
   const auto first = compute_scc(g);
   const auto second = compute_scc(g);
   REQUIRE(first == second);
}

TEST_CASE("reachable_from walks forward edges", "[graph][algorithms][reachability]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   const NodeId d = g.add_node("d.h");
   g.add_edge(a, b);
   g.add_edge(b, c);
   const auto r = reachable_from(g, a);
   REQUIRE(r.size() == 3);
   REQUIRE(r.count(a) == 1);
   REQUIRE(r.count(b) == 1);
   REQUIRE(r.count(c) == 1);
   REQUIRE(r.count(d) == 0);
}

TEST_CASE("reachable_from on isolated node returns just itself", "[graph][algorithms][reachability]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const auto r = reachable_from(g, a);
   REQUIRE(r.size() == 1);
   REQUIRE(r.count(a) == 1);
}

TEST_CASE("reverse_reachable_from mirrors forward reachability", "[graph][algorithms][reachability]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, b);
   g.add_edge(b, c);
   const auto fwd = reachable_from(g, a);
   const auto rev = reverse_reachable_from(g, c);
   REQUIRE(fwd == rev);
}

TEST_CASE("has_path returns true for direct edge", "[graph][algorithms][path]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   g.add_edge(a, b);
   REQUIRE(has_path(g, a, b));
}

TEST_CASE("has_path returns true for transitive reachability", "[graph][algorithms][path]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, b);
   g.add_edge(b, c);
   REQUIRE(has_path(g, a, c));
}

TEST_CASE("has_path returns false for unreachable target", "[graph][algorithms][path]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   REQUIRE_FALSE(has_path(g, a, b));
}

TEST_CASE("has_path returns true for self-path", "[graph][algorithms][path]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   REQUIRE(has_path(g, a, a));
}

TEST_CASE("has_path respects edge direction", "[graph][algorithms][path]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   g.add_edge(a, b);
   REQUIRE_FALSE(has_path(g, b, a));
}
