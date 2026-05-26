#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

using archcheck::graph::DependencyGraph;
using archcheck::graph::NodeId;

TEST_CASE("add_node returns the same NodeId for the same path", "[graph][container]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("foo/bar.h");
   const NodeId b = g.add_node("foo/bar.h");
   REQUIRE(a == b);
   REQUIRE(g.node_count() == 1);
}

TEST_CASE("add_node assigns distinct ids to distinct paths", "[graph][container]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   REQUIRE(a != b);
   REQUIRE(g.node_count() == 2);
}

TEST_CASE("path_of round-trips the normalized path", "[graph][container]")
{
   DependencyGraph g;
   const NodeId id = g.add_node("foo/bar.h");
   REQUIRE(g.path_of(id) == "foo/bar.h");
}

TEST_CASE("add_node normalizes leading ./ and backslashes", "[graph][container][normalize]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("foo/bar.h");
   const NodeId b = g.add_node("./foo/bar.h");
   const NodeId c = g.add_node("foo\\bar.h");
   REQUIRE(a == b);
   REQUIRE(a == c);
   REQUIRE(g.node_count() == 1);
   REQUIRE(g.path_of(a) == "foo/bar.h");
}

TEST_CASE("add_node strips multiple ./ prefixes", "[graph][container][normalize]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("foo.h");
   const NodeId b = g.add_node("././foo.h");
   REQUIRE(a == b);
}

TEST_CASE("has_edge is false for fresh graph", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   REQUIRE_FALSE(g.has_edge(a, b));
}

TEST_CASE("add_edge then has_edge returns true", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   g.add_edge(a, b);
   REQUIRE(g.has_edge(a, b));
   REQUIRE_FALSE(g.has_edge(b, a));
}

TEST_CASE("add_edge is idempotent", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   g.add_edge(a, b);
   g.add_edge(a, b);
   g.add_edge(a, b);
   REQUIRE(g.successors(a).size() == 1);
   REQUIRE(g.predecessors(b).size() == 1);
}

TEST_CASE("successors returns the forward adjacency list", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, b);
   g.add_edge(a, c);
   const auto& succ = g.successors(a);
   REQUIRE(succ.size() == 2);
   REQUIRE(succ[0] == b);
   REQUIRE(succ[1] == c);
}

TEST_CASE("predecessors mirrors the forward edges", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   const NodeId b = g.add_node("b.h");
   const NodeId c = g.add_node("c.h");
   g.add_edge(a, c);
   g.add_edge(b, c);
   const auto& pred = g.predecessors(c);
   REQUIRE(pred.size() == 2);
   REQUIRE(pred[0] == a);
   REQUIRE(pred[1] == b);
}

TEST_CASE("successors and predecessors return empty for orphan nodes", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   REQUIRE(g.successors(a).empty());
   REQUIRE(g.predecessors(a).empty());
}

TEST_CASE("self-loop is recorded in both adjacency lists", "[graph][container][edges]")
{
   DependencyGraph g;
   const NodeId a = g.add_node("a.h");
   g.add_edge(a, a);
   REQUIRE(g.has_edge(a, a));
   REQUIRE(g.successors(a).size() == 1);
   REQUIRE(g.predecessors(a).size() == 1);
}
