#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/diff.h"
#include "archcheck/scan/include_resolver.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"

using archcheck::graph::addedEdges;
using archcheck::graph::computeScc;
using archcheck::graph::DependencyGraph;
using archcheck::graph::grownSccs;
using archcheck::graph::NodeId;
using archcheck::scan::Resolution;
using archcheck::scan::resolveIncludes;
using archcheck::scan::scanIncludes;

namespace
{

std::filesystem::path graph_fixture(std::string_view name)
{
  return std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / "graph" / name;
}

std::string read_file(const std::filesystem::path &p)
{
  std::ifstream f(p, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

struct BuildResult
{
  DependencyGraph graph;
  std::size_t external = 0;
  std::size_t unresolved = 0;
  std::size_t ambiguous = 0;
};

BuildResult build_graph(const std::filesystem::path &root)
{
  const auto files = archcheck::scan::discoverFiles(root);
  const auto index = archcheck::scan::buildProjectIndex(files);
  BuildResult out;
  std::vector<NodeId> id_map;
  id_map.reserve(files.size());
  for (const auto &f : files)
  {
    id_map.push_back(out.graph.addNode(f.path));
  }
  for (std::size_t i = 0; i < files.size(); ++i)
  {
    const auto src = read_file(root / files[i].path);
    const auto scanned = scanIncludes(src);
    const auto resolved = resolveIncludes(scanned.directives, files[i].path, files, index);
    for (const auto &r : resolved)
    {
      if (r.resolution == Resolution::Project)
        out.graph.addEdge(id_map[i], id_map[r.target]);
      else if (r.resolution == Resolution::External)
        ++out.external;
      else if (r.resolution == Resolution::Unresolved)
        ++out.unresolved;
      else if (r.resolution == Resolution::Ambiguous)
        ++out.ambiguous;
    }
  }
  return out;
}

} // namespace

TEST_CASE("fixture: minimal_dag — 3 nodes, 2 edges, acyclic", "[graph][fixtures]")
{
  const auto r = build_graph(graph_fixture("minimal_dag"));
  REQUIRE(r.graph.nodeCount() == 3);
  const auto sccs = computeScc(r.graph);
  REQUIRE(sccs.size() == 3);
  for (const auto &c : sccs)
    REQUIRE(c.size() == 1);
}

TEST_CASE("fixture: single_scc — 3 nodes форма one SCC", "[graph][fixtures]")
{
  const auto r = build_graph(graph_fixture("single_scc"));
  REQUIRE(r.graph.nodeCount() == 3);
  const auto sccs = computeScc(r.graph);
  std::size_t big = 0;
  for (const auto &c : sccs)
    if (c.size() >= 2)
      ++big;
  REQUIRE(big == 1);
}

TEST_CASE("fixture: new_edge — diff показывает одно новое ребро", "[graph][fixtures]")
{
  const auto base = build_graph(graph_fixture("new_edge/baseline"));
  const auto curr = build_graph(graph_fixture("new_edge/current"));
  const auto added = addedEdges(base.graph, curr.graph);
  REQUIRE(added.size() == 1);
  REQUIRE(curr.graph.pathOf(added[0].from) == "a.h");
  REQUIRE(curr.graph.pathOf(added[0].to) == "c.h");
}

TEST_CASE("fixture: shortcut_edge — diff показывает shortcut поверх a->b->c->d", "[graph][fixtures]")
{
  const auto base = build_graph(graph_fixture("shortcut_edge/baseline"));
  const auto curr = build_graph(graph_fixture("shortcut_edge/current"));
  const auto added = addedEdges(base.graph, curr.graph);
  REQUIRE(added.size() == 1);
  REQUIRE(curr.graph.pathOf(added[0].from) == "a.h");
  REQUIRE(curr.graph.pathOf(added[0].to) == "d.h");
}

TEST_CASE("fixture: cycle_growth — SCC размер 2 -> 3", "[graph][fixtures]")
{
  const auto base = build_graph(graph_fixture("cycle_growth/baseline"));
  const auto curr = build_graph(graph_fixture("cycle_growth/current"));
  const auto grown = grownSccs(base.graph, curr.graph);
  REQUIRE(grown.size() == 1);
  REQUIRE(grown[0].baselineSize == 2);
  REQUIRE(grown[0].currentSize == 3);
}

TEST_CASE("fixture: unresolved_include — попадает в diagnostics, не в edges", "[graph][fixtures]")
{
  const auto r = build_graph(graph_fixture("unresolved_include"));
  REQUIRE(r.graph.nodeCount() == 1);
  REQUIRE(r.unresolved == 1);
  REQUIRE(computeScc(r.graph).size() == 1);
}

TEST_CASE("fixture: ambiguous_include — 2 кандидата, edge не строится", "[graph][fixtures]")
{
  const auto r = build_graph(graph_fixture("ambiguous_include"));
  REQUIRE(r.graph.nodeCount() == 3);
  REQUIRE(r.ambiguous == 1);
  REQUIRE(r.unresolved == 0);
}
