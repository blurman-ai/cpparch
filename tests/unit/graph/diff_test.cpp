#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/diff.h"
#include "archcheck/graph/node_id.h"

using archcheck::graph::addedEdges;
using archcheck::graph::DependencyGraph;
using archcheck::graph::EdgeRef;
using archcheck::graph::grownSccs;
using archcheck::graph::NodeId;
using archcheck::graph::removedEdges;

namespace
{

DependencyGraph make_chain_abc()
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

TEST_CASE("addedEdges reports a brand new edge", "[graph][diff][added]")
{
  const DependencyGraph baseline = make_chain_abc();
  DependencyGraph current = make_chain_abc();
  const NodeId a = current.addNode("a.h");
  const NodeId c = current.addNode("c.h");
  current.addEdge(a, c);

  const auto added = addedEdges(baseline, current);
  REQUIRE(added.size() == 1);
  REQUIRE(current.pathOf(added[0].from) == "a.h");
  REQUIRE(current.pathOf(added[0].to) == "c.h");
}

TEST_CASE("addedEdges flags a shortcut edge introduced in current", "[graph][diff][added]")
{
  DependencyGraph baseline;
  const NodeId ba = baseline.addNode("a.h");
  const NodeId bb = baseline.addNode("b.h");
  const NodeId bc = baseline.addNode("c.h");
  baseline.addEdge(ba, bb);
  baseline.addEdge(bb, bc);

  DependencyGraph current;
  const NodeId ca = current.addNode("a.h");
  const NodeId cb = current.addNode("b.h");
  const NodeId cc = current.addNode("c.h");
  current.addEdge(ca, cb);
  current.addEdge(cb, cc);
  current.addEdge(ca, cc); // shortcut

  const auto added = addedEdges(baseline, current);
  REQUIRE(added.size() == 1);
  REQUIRE(current.pathOf(added[0].from) == "a.h");
  REQUIRE(current.pathOf(added[0].to) == "c.h");
}

TEST_CASE("addedEdges sees edges to brand-new nodes", "[graph][diff][added]")
{
  const DependencyGraph baseline = make_chain_abc();
  DependencyGraph current = make_chain_abc();
  const NodeId c = current.addNode("c.h");
  const NodeId d = current.addNode("d.h");
  current.addEdge(c, d);

  const auto added = addedEdges(baseline, current);
  REQUIRE(added.size() == 1);
  REQUIRE(current.pathOf(added[0].from) == "c.h");
  REQUIRE(current.pathOf(added[0].to) == "d.h");
}

TEST_CASE("removedEdges reports a vanished edge", "[graph][diff][removed]")
{
  const DependencyGraph baseline = make_chain_abc();
  DependencyGraph current;
  const NodeId a = current.addNode("a.h");
  const NodeId b = current.addNode("b.h");
  current.addNode("c.h");
  current.addEdge(a, b);
  // b -> c missing

  const auto removed = removedEdges(baseline, current);
  REQUIRE(removed.size() == 1);
  REQUIRE(current.pathOf(removed[0].from) == "b.h");
  REQUIRE(current.pathOf(removed[0].to) == "c.h");
}

TEST_CASE("removedEdges skips edges whose endpoint disappeared", "[graph][diff][removed]")
{
  const DependencyGraph baseline = make_chain_abc();
  DependencyGraph current;
  const NodeId a = current.addNode("a.h");
  const NodeId b = current.addNode("b.h");
  current.addEdge(a, b);
  // c.h removed entirely; the b->c edge has no current endpoint and is dropped

  const auto removed = removedEdges(baseline, current);
  REQUIRE(removed.empty());
}

TEST_CASE("addedEdges and removedEdges return empty for identical graphs", "[graph][diff][noop]")
{
  const DependencyGraph baseline = make_chain_abc();
  const DependencyGraph current = make_chain_abc();
  REQUIRE(addedEdges(baseline, current).empty());
  REQUIRE(removedEdges(baseline, current).empty());
}

TEST_CASE("grownSccs reports nothing when both graphs are acyclic", "[graph][diff][scc]")
{
  const DependencyGraph baseline = make_chain_abc();
  const DependencyGraph current = make_chain_abc();
  REQUIRE(grownSccs(baseline, current).empty());
}

TEST_CASE("grownSccs flags a brand-new cycle", "[graph][diff][scc]")
{
  const DependencyGraph baseline = make_chain_abc();
  DependencyGraph current = make_chain_abc();
  const NodeId a = current.addNode("a.h");
  const NodeId c = current.addNode("c.h");
  current.addEdge(c, a); // creates a-b-c cycle

  const auto grown = grownSccs(baseline, current);
  REQUIRE(grown.size() == 1);
  REQUIRE(grown[0].baselineSize == 0);
  REQUIRE(grown[0].currentSize == 3);
  REQUIRE(grown[0].members.size() == 3);
}

TEST_CASE("grownSccs flags a cycle that grew larger", "[graph][diff][scc]")
{
  DependencyGraph baseline;
  const NodeId ba = baseline.addNode("a.h");
  const NodeId bb = baseline.addNode("b.h");
  baseline.addEdge(ba, bb);
  baseline.addEdge(bb, ba); // 2-cycle

  DependencyGraph current;
  const NodeId ca = current.addNode("a.h");
  const NodeId cb = current.addNode("b.h");
  const NodeId cc = current.addNode("c.h");
  current.addEdge(ca, cb);
  current.addEdge(cb, cc);
  current.addEdge(cc, ca); // 3-cycle, overlaps baseline by {a, b}

  const auto grown = grownSccs(baseline, current);
  REQUIRE(grown.size() == 1);
  REQUIRE(grown[0].baselineSize == 2);
  REQUIRE(grown[0].currentSize == 3);
}

TEST_CASE("grownSccs ignores a cycle of unchanged size", "[graph][diff][scc]")
{
  DependencyGraph baseline;
  const NodeId ba = baseline.addNode("a.h");
  const NodeId bb = baseline.addNode("b.h");
  baseline.addEdge(ba, bb);
  baseline.addEdge(bb, ba);

  DependencyGraph current;
  const NodeId ca = current.addNode("a.h");
  const NodeId cb = current.addNode("b.h");
  current.addEdge(ca, cb);
  current.addEdge(cb, ca);

  REQUIRE(grownSccs(baseline, current).empty());
}
