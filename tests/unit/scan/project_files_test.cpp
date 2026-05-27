#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "archcheck/scan/project_files.h"

using archcheck::scan::buildProjectIndex;
using archcheck::scan::discoverFiles;
using archcheck::scan::NodeId;
using archcheck::scan::ProjectFile;

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

TempTree make_tree(std::string_view tag)
{
  static std::atomic<int> counter{0};
  const auto base = std::filesystem::temp_directory_path() /
                    ("archcheck_pf_" + std::string{tag} + "_" + std::to_string(counter.fetch_add(1)));
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::filesystem::create_directories(base);
  return TempTree{base};
}

void touch(const std::filesystem::path &p)
{
  std::filesystem::create_directories(p.parent_path());
  std::ofstream{p} << "x";
}

std::vector<std::string> paths_of(const std::vector<ProjectFile> &files)
{
  std::vector<std::string> out;
  out.reserve(files.size());
  for (const auto &f : files)
  {
    out.push_back(f.path);
  }
  return out;
}

bool contains(const std::vector<std::string> &v, std::string_view s)
{
  return std::find(v.begin(), v.end(), s) != v.end();
}

} // namespace

TEST_CASE("discover_files on empty dir returns empty", "[scan][project_files]")
{
  auto tree = make_tree("empty");
  REQUIRE(discoverFiles(tree.root).empty());
}

TEST_CASE("discover_files collects files with project extensions", "[scan][project_files]")
{
  auto tree = make_tree("ext");
  touch(tree.root / "a.cpp");
  touch(tree.root / "b.h");
  touch(tree.root / "sub" / "c.hpp");
  touch(tree.root / "ignored.txt");
  touch(tree.root / "no_ext");

  const auto paths = paths_of(discoverFiles(tree.root));
  REQUIRE(paths.size() == 3);
  REQUIRE(contains(paths, "a.cpp"));
  REQUIRE(contains(paths, "b.h"));
  REQUIRE(contains(paths, "sub/c.hpp"));
}

TEST_CASE("discover_files accepts the full v0.1 extension set", "[scan][project_files]")
{
  auto tree = make_tree("allext");
  for (auto e : {"a.c", "a.cc", "a.cpp", "a.cxx", "a.h", "a.hh", "a.hpp", "a.hxx", "a.ipp", "a.tpp", "a.inl", "a.inc"})
  {
    touch(tree.root / e);
  }
  REQUIRE(discoverFiles(tree.root).size() == 12);
}

TEST_CASE("discover_files skips excluded directories", "[scan][project_files]")
{
  auto tree = make_tree("excl");
  touch(tree.root / "keep.cpp");
  for (auto d : {".git", "build", ".cache", ".idea", ".vscode", "out", "cmake-build-debug", "cmake-build-x"})
  {
    touch(tree.root / d / "skip.cpp");
  }
  const auto paths = paths_of(discoverFiles(tree.root));
  REQUIRE(paths.size() == 1);
  REQUIRE(paths[0] == "keep.cpp");
}

TEST_CASE("discover_files does NOT auto-exclude third_party / vendor", "[scan][project_files]")
{
  auto tree = make_tree("3p");
  touch(tree.root / "third_party" / "x.cpp");
  touch(tree.root / "vendor" / "y.h");
  const auto paths = paths_of(discoverFiles(tree.root));
  REQUIRE(paths.size() == 2);
  REQUIRE(contains(paths, "third_party/x.cpp"));
  REQUIRE(contains(paths, "vendor/y.h"));
}

TEST_CASE("discover_files returns POSIX-normalized repo-relative paths", "[scan][project_files]")
{
  auto tree = make_tree("posix");
  touch(tree.root / "a" / "b" / "c.h");
  const auto paths = paths_of(discoverFiles(tree.root));
  REQUIRE(paths.size() == 1);
  REQUIRE(paths[0] == "a/b/c.h");
}

TEST_CASE("discover_files sorts results deterministically", "[scan][project_files]")
{
  auto tree = make_tree("sort");
  touch(tree.root / "z.cpp");
  touch(tree.root / "a.cpp");
  touch(tree.root / "m" / "n.h");
  const auto paths = paths_of(discoverFiles(tree.root));
  REQUIRE(paths == std::vector<std::string>{"a.cpp", "m/n.h", "z.cpp"});
}

TEST_CASE("build_project_index: empty input yields empty indexes", "[scan][project_files][index]")
{
  const auto idx = buildProjectIndex({});
  REQUIRE(idx.exactPathIndex.empty());
  REQUIRE(idx.suffixIndex.empty());
}

TEST_CASE("build_project_index: exact lookup finds the file by repo-relative path", "[scan][project_files][index]")
{
  const std::vector<ProjectFile> files = {{"a/b/c.h"}, {"x.cpp"}};
  const auto idx = buildProjectIndex(files);
  REQUIRE(idx.exactPathIndex.at("a/b/c.h") == NodeId{0});
  REQUIRE(idx.exactPathIndex.at("x.cpp") == NodeId{1});
  REQUIRE(idx.exactPathIndex.find("c.h") == idx.exactPathIndex.end());
}

TEST_CASE("build_project_index: suffix index contains every '/'-segment suffix", "[scan][project_files][index]")
{
  const std::vector<ProjectFile> files = {{"a/b/c.h"}};
  const auto idx = buildProjectIndex(files);
  REQUIRE(idx.suffixIndex.at("a/b/c.h") == std::vector<NodeId>{0});
  REQUIRE(idx.suffixIndex.at("b/c.h") == std::vector<NodeId>{0});
  REQUIRE(idx.suffixIndex.at("c.h") == std::vector<NodeId>{0});
  REQUIRE(idx.suffixIndex.find("/c.h") == idx.suffixIndex.end());
}

TEST_CASE("build_project_index: suffix collisions list all candidates", "[scan][project_files][index]")
{
  const std::vector<ProjectFile> files = {{"foo/util.h"}, {"bar/util.h"}};
  const auto idx = buildProjectIndex(files);
  auto candidates = idx.suffixIndex.at("util.h");
  std::sort(candidates.begin(), candidates.end());
  REQUIRE(candidates == std::vector<NodeId>{0, 1});
  REQUIRE(idx.suffixIndex.at("foo/util.h") == std::vector<NodeId>{0});
  REQUIRE(idx.suffixIndex.at("bar/util.h") == std::vector<NodeId>{1});
}

TEST_CASE("build_project_index: single-segment path indexes only itself", "[scan][project_files][index]")
{
  const std::vector<ProjectFile> files = {{"main.cpp"}};
  const auto idx = buildProjectIndex(files);
  REQUIRE(idx.exactPathIndex.at("main.cpp") == NodeId{0});
  REQUIRE(idx.suffixIndex.size() == 1);
  REQUIRE(idx.suffixIndex.at("main.cpp") == std::vector<NodeId>{0});
}
