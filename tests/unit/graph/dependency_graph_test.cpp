#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

using archcheck::graph::DependencyGraph;
using archcheck::graph::NodeId;

TEST_CASE("addNode returns the same NodeId for the same path", "[graph][container]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("foo/bar.h");
  const NodeId b = g.addNode("foo/bar.h");
  REQUIRE(a == b);
  REQUIRE(g.nodeCount() == 1);
}

TEST_CASE("addNode assigns distinct ids to distinct paths", "[graph][container]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  REQUIRE(a != b);
  REQUIRE(g.nodeCount() == 2);
}

TEST_CASE("pathOf round-trips the normalized path", "[graph][container]")
{
  DependencyGraph g;
  const NodeId id = g.addNode("foo/bar.h");
  REQUIRE(g.pathOf(id) == "foo/bar.h");
}

TEST_CASE("addNode normalizes leading ./ and backslashes", "[graph][container][normalize]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("foo/bar.h");
  const NodeId b = g.addNode("./foo/bar.h");
  const NodeId c = g.addNode("foo\\bar.h");
  REQUIRE(a == b);
  REQUIRE(a == c);
  REQUIRE(g.nodeCount() == 1);
  REQUIRE(g.pathOf(a) == "foo/bar.h");
}

TEST_CASE("addNode strips multiple ./ prefixes", "[graph][container][normalize]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("foo.h");
  const NodeId b = g.addNode("././foo.h");
  REQUIRE(a == b);
}

TEST_CASE("hasEdge is false for fresh graph", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  REQUIRE_FALSE(g.hasEdge(a, b));
}

TEST_CASE("addEdge then hasEdge returns true", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  g.addEdge(a, b);
  REQUIRE(g.hasEdge(a, b));
  REQUIRE_FALSE(g.hasEdge(b, a));
}

TEST_CASE("addEdge is idempotent", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  g.addEdge(a, b);
  g.addEdge(a, b);
  g.addEdge(a, b);
  REQUIRE(g.successors(a).size() == 1);
  REQUIRE(g.predecessors(b).size() == 1);
}

TEST_CASE("successors returns the forward adjacency list", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, b);
  g.addEdge(a, c);
  const auto &succ = g.successors(a);
  REQUIRE(succ.size() == 2);
  REQUIRE(succ[0] == b);
  REQUIRE(succ[1] == c);
}

TEST_CASE("predecessors mirrors the forward edges", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  const NodeId b = g.addNode("b.h");
  const NodeId c = g.addNode("c.h");
  g.addEdge(a, c);
  g.addEdge(b, c);
  const auto &pred = g.predecessors(c);
  REQUIRE(pred.size() == 2);
  REQUIRE(pred[0] == a);
  REQUIRE(pred[1] == b);
}

TEST_CASE("successors and predecessors return empty for orphan nodes", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  REQUIRE(g.successors(a).empty());
  REQUIRE(g.predecessors(a).empty());
}

TEST_CASE("self-loop is recorded in both adjacency lists", "[graph][container][edges]")
{
  DependencyGraph g;
  const NodeId a = g.addNode("a.h");
  g.addEdge(a, a);
  REQUIRE(g.hasEdge(a, a));
  REQUIRE(g.successors(a).size() == 1);
  REQUIRE(g.predecessors(a).size() == 1);
}
