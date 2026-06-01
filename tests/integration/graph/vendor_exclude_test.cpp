#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "archcheck/graph/graph_builder.h"

// #069/#068: vendored single-file libs and vendor directories must not enter the
// include graph — their cycles and fan-in are noise, not author drift.

using archcheck::graph::buildGraphForPath;

namespace
{

struct TempTree
{
  std::filesystem::path root;
  ~TempTree()
  {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }
};

TempTree make_tree()
{
  static std::atomic<int> counter{0};
  const auto base =
      std::filesystem::temp_directory_path() / ("archcheck_vendor_" + std::to_string(counter.fetch_add(1)));
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::filesystem::create_directories(base);
  return TempTree{base};
}

void write(const std::filesystem::path &p, const std::string &content)
{
  std::filesystem::create_directories(p.parent_path());
  std::ofstream{p} << content;
}

bool graph_contains(const archcheck::graph::DependencyGraph &g, std::string_view needle)
{
  for (std::uint32_t i = 0; i < g.nodeCount(); ++i)
  {
    if (g.pathOf(archcheck::graph::NodeId{i}).find(needle) != std::string_view::npos)
    {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("graph excludes vendored single-file lib by curated name", "[graph][vendor]")
{
  const TempTree tree = make_tree();
  write(tree.root / "app.cpp", "#include \"util.h\"\n");
  write(tree.root / "util.h", "// author util\n");
  // qcustomplot.* is a curated single-file lib (layer 1) dropped straight into src.
  write(tree.root / "qcustomplot.h", "#include \"qcustomplot.cpp\"\n");
  write(tree.root / "qcustomplot.cpp", "#include \"qcustomplot.h\"\n"); // self-cycle

  const auto built = buildGraphForPath(tree.root);

  REQUIRE(graph_contains(built.graph, "app.cpp"));
  REQUIRE(graph_contains(built.graph, "util.h"));
  REQUIRE_FALSE(graph_contains(built.graph, "qcustomplot"));
  REQUIRE(built.graph.nodeCount() == 2);
}

TEST_CASE("graph excludes vendored lib by permissive-license header", "[graph][vendor]")
{
  const TempTree tree = make_tree();
  write(tree.root / "app.cpp", "// author code\n");
  // Renamed/unknown vendor caught only by the license banner (layer 2).
  write(tree.root / "renamed_lib.h", "// SPDX-License-Identifier: MIT\nint vendored();\n");

  const auto built = buildGraphForPath(tree.root);

  REQUIRE(graph_contains(built.graph, "app.cpp"));
  REQUIRE_FALSE(graph_contains(built.graph, "renamed_lib"));
}

TEST_CASE("graph excludes vendored directory subtrees", "[graph][vendor]")
{
  const TempTree tree = make_tree();
  write(tree.root / "src" / "main.cpp", "// author\n");
  write(tree.root / "third_party" / "fmt" / "format.h", "// vendor\n");
  write(tree.root / "extern" / "sdl" / "sdl.h", "// vendor\n");

  const auto built = buildGraphForPath(tree.root);

  REQUIRE(graph_contains(built.graph, "src/main.cpp"));
  REQUIRE_FALSE(graph_contains(built.graph, "third_party"));
  REQUIRE_FALSE(graph_contains(built.graph, "extern"));
  REQUIRE(built.graph.nodeCount() == 1);
}
