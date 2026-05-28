#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <sstream>
#include <string>

#include "archcheck/graph/baseline.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

using archcheck::graph::BaselineLoadError;
using archcheck::graph::DependencyGraph;
using archcheck::graph::loadBaseline;
using archcheck::graph::NodeId;
using archcheck::graph::saveBaseline;

namespace
{

std::string save_to_string(const DependencyGraph &g)
{
  std::ostringstream os;
  saveBaseline(g, os);
  return os.str();
}

DependencyGraph make_sample_a()
{
  DependencyGraph g;
  const NodeId a = g.addNode("include/a.h");
  const NodeId b = g.addNode("include/b.h");
  const NodeId c = g.addNode("src/c.cpp");
  g.addEdge(c, a);
  g.addEdge(c, b);
  g.addEdge(a, b);
  return g;
}

DependencyGraph make_sample_a_reordered()
{
  DependencyGraph g;
  const NodeId c = g.addNode("src/c.cpp");
  const NodeId b = g.addNode("include/b.h");
  const NodeId a = g.addNode("include/a.h");
  g.addEdge(a, b);
  g.addEdge(c, b);
  g.addEdge(c, a);
  return g;
}

} // namespace

TEST_CASE("saveBaseline → loadBaseline → saveBaseline is idempotent", "[graph][baseline][roundtrip]")
{
  const DependencyGraph g = make_sample_a();
  const std::string first = save_to_string(g);

  std::istringstream is(first);
  auto [loaded, err] = loadBaseline(is);
  REQUIRE_FALSE(err.has_value());

  const std::string second = save_to_string(loaded);
  REQUIRE(first == second);
}

TEST_CASE("saveBaseline is deterministic across insertion orders", "[graph][baseline][determinism]")
{
  const std::string a = save_to_string(make_sample_a());
  const std::string b = save_to_string(make_sample_a_reordered());
  REQUIRE(a == b);
}

TEST_CASE("saveBaseline emits sorted nodes and edges", "[graph][baseline][determinism]")
{
  const std::string text = save_to_string(make_sample_a());
  const auto pos_a = text.find("include/a.h");
  const auto pos_b = text.find("include/b.h");
  const auto pos_c = text.find("src/c.cpp");
  REQUIRE(pos_a != std::string::npos);
  REQUIRE(pos_a < pos_b);
  REQUIRE(pos_b < pos_c);
}

TEST_CASE("loadBaseline rejects unknown format_version", "[graph][baseline][version]")
{
  const std::string text = "format_version: \"999\"\n"
                           "nodes: []\n"
                           "edges: []\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::UnknownVersion);
}

TEST_CASE("loadBaseline reports parse error on malformed YAML", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\nnodes: [unterminated\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::ParseError);
}

TEST_CASE("loadBaseline reports MissingField when nodes absent", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\n"
                           "edges: []\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MissingField);
}

TEST_CASE("loadBaseline reports MissingField when edges absent", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\n"
                           "nodes: []\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MissingField);
}

TEST_CASE("loadBaseline reports MissingField when format_version absent", "[graph][baseline][errors]")
{
  const std::string text = "nodes: []\n"
                           "edges: []\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MissingField);
}

TEST_CASE("loadBaseline reports MalformedSchema on out-of-range edge", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\n"
                           "nodes:\n"
                           "   - \"a.h\"\n"
                           "edges:\n"
                           "   - [0, 5]\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MalformedSchema);
}

TEST_CASE("loadBaseline reports MalformedSchema when top-level is not a mapping", "[graph][baseline][errors]")
{
  const std::string text = "- just a sequence\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MalformedSchema);
}

TEST_CASE("loadBaseline reports MalformedSchema when format_version is not a scalar", "[graph][baseline][errors]")
{
  const std::string text = "format_version:\n"
                           "  - nested: value\n"
                           "nodes: []\n"
                           "edges: []\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MalformedSchema);
}

TEST_CASE("loadBaseline reports MalformedSchema when nodes is not a sequence", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\n"
                           "nodes: scalar_value\n"
                           "edges: []\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MalformedSchema);
}

TEST_CASE("loadBaseline reports MalformedSchema when edge is not a pair", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\n"
                           "nodes:\n"
                           "   - \"a.h\"\n"
                           "   - \"b.h\"\n"
                           "edges:\n"
                           "   - [0, 1, 0]\n"; // triple, not pair
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MalformedSchema);
}

TEST_CASE("loadBaseline reports MalformedSchema when edge index is not integer", "[graph][baseline][errors]")
{
  const std::string text = "format_version: \"1\"\n"
                           "nodes:\n"
                           "   - \"a.h\"\n"
                           "edges:\n"
                           "   - [abc, 0]\n";
  std::istringstream is(text);
  auto [g, err] = loadBaseline(is);
  REQUIRE(err.has_value());
  REQUIRE(err->kind == BaselineLoadError::Kind::MalformedSchema);
}

TEST_CASE("saveBaseline handles empty graph (no nodes, no edges)", "[graph][baseline][roundtrip]")
{
  DependencyGraph empty;
  const std::string text = save_to_string(empty);
  REQUIRE(text.find("nodes:") != std::string::npos);
  REQUIRE(text.find("edges:") != std::string::npos);

  std::istringstream is(text);
  auto [loaded, err] = loadBaseline(is);
  REQUIRE_FALSE(err.has_value());
  REQUIRE(loaded.nodeCount() == 0);
}

TEST_CASE("loadBaseline restores the original graph topology", "[graph][baseline][roundtrip]")
{
  const DependencyGraph g = make_sample_a();
  const std::string text = save_to_string(g);

  std::istringstream is(text);
  auto [loaded, err] = loadBaseline(is);
  REQUIRE_FALSE(err.has_value());
  REQUIRE(loaded.nodeCount() == 3);

  const NodeId a = loaded.addNode("include/a.h");
  const NodeId b = loaded.addNode("include/b.h");
  const NodeId c = loaded.addNode("src/c.cpp");
  REQUIRE(loaded.hasEdge(c, a));
  REQUIRE(loaded.hasEdge(c, b));
  REQUIRE(loaded.hasEdge(a, b));
  REQUIRE_FALSE(loaded.hasEdge(a, c));
}
