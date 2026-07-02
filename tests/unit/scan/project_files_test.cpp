#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/file_source.h"
#include "archcheck/scan/project_files.h"

using archcheck::scan::buildProjectIndex;
using archcheck::scan::collectNonVendoredSources;
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
  for (auto e :
       {"a.c", "a.C", "a.cc", "a.cpp", "a.cxx", "a.h", "a.hh", "a.hpp", "a.hxx", "a.ipp", "a.tpp", "a.inl", "a.inc"})
  {
    touch(tree.root / e);
  }
  REQUIRE(discoverFiles(tree.root).size() == 13);
}

TEST_CASE("discover_files recognises uppercase .C as C++ source (#131)", "[scan][project_files]")
{
  // GCC convention: `.C` (uppercase) is C++ source, distinct from `.c` on
  // case-sensitive filesystems — corpus repos using it were silently unscanned.
  auto tree = make_tree("uppercase_c");
  touch(tree.root / "Widget.C");
  const auto paths = paths_of(discoverFiles(tree.root));
  REQUIRE(paths.size() == 1);
  REQUIRE(contains(paths, "Widget.C"));
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

TEST_CASE("discover_files survives a self-referential directory symlink (#081)", "[scan][project_files]")
{
  auto tree = make_tree("symloop");
  touch(tree.root / "real.cpp");
  touch(tree.root / "sub" / "nested.h");
  std::error_code ec;
  // CodeQL-style self-loop: a directory symlink pointing back at its own parent.
  // Before #081 this spun recursive_directory_iterator and the walk returned 0.
  std::filesystem::create_directory_symlink(tree.root, tree.root / "loop", ec);
  if (ec)
  {
    SUCCEED("filesystem does not support directory symlinks here");
    return;
  }
  const auto paths = paths_of(discoverFiles(tree.root));
  // Walk must terminate and surface the real files (not 0), without descending
  // into the symlinked dir (else "loop/real.cpp" would double-count).
  REQUIRE(paths.size() == 2);
  REQUIRE(contains(paths, "real.cpp"));
  REQUIRE(contains(paths, "sub/nested.h"));
}

TEST_CASE("discover_files rejects file symlink pointing outside root (S3)", "[scan][project_files][security]")
{
  auto tree = make_tree("symfile");
  touch(tree.root / "real.cpp");
  // Create a real file outside the tree to be the symlink target.
  auto outside = make_tree("symfile_outside");
  touch(outside.root / "secret.h");
  const auto target = outside.root / "secret.h";
  std::error_code ec;
  std::filesystem::create_symlink(target, tree.root / "evil.h", ec);
  if (ec)
  {
    SUCCEED("filesystem does not support symlinks here");
    return;
  }
  const auto paths = paths_of(discoverFiles(tree.root));
  // evil.h must NOT appear — it points outside the root.
  REQUIRE(!contains(paths, "evil.h"));
  REQUIRE(contains(paths, "real.cpp"));
}

TEST_CASE("discover_files accepts file symlink pointing inside root (S3)", "[scan][project_files][security]")
{
  auto tree = make_tree("symfile_in");
  touch(tree.root / "real.cpp");
  touch(tree.root / "actual.h");
  std::error_code ec;
  // Symlink inside the same tree — this is acceptable.
  std::filesystem::create_symlink(tree.root / "actual.h", tree.root / "alias.h", ec);
  if (ec)
  {
    SUCCEED("filesystem does not support symlinks here");
    return;
  }
  const auto paths = paths_of(discoverFiles(tree.root));
  // Both the real file and the in-tree symlink are accepted.
  REQUIRE(contains(paths, "real.cpp"));
  REQUIRE(contains(paths, "actual.h"));
  REQUIRE(contains(paths, "alias.h"));
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

namespace
{
// In-memory FileSource: returns the configured files and their content.
struct FakeSource : archcheck::scan::FileSource
{
  std::vector<ProjectFile> files;
  std::vector<std::pair<std::string, std::string>> blobs;
  std::vector<ProjectFile> list() override { return files; }
  std::string read(const std::string &path) override
  {
    for (const auto &b : blobs)
    {
      if (b.first == path)
      {
        return b.second;
      }
    }
    return "";
  }
};
} // namespace

TEST_CASE("collect_non_vendored_sources: drops vendored + empty, keeps project code", "[scan][vendor]")
{
  FakeSource src;
  src.files = {{"src/app/foo.cpp"}, {"third_party/lib/bar.cpp"}, {"src/empty.cpp"}};
  src.blobs = {
      {"src/app/foo.cpp", "int foo() { return 1; }"},
      {"third_party/lib/bar.cpp", "int bar() { return 2; }"}, // vendored directory segment
      {"src/empty.cpp", ""},                                  // empty
  };

  const auto out = collectNonVendoredSources(src);

  REQUIRE(out.size() == 1);
  REQUIRE(out[0].first == "src/app/foo.cpp");
}

TEST_CASE("collect_non_vendored_sources: drops unit-test code (#070)", "[scan][test]")
{
  FakeSource src;
  src.files = {{"src/app/foo.cpp"}, {"tests/unit/foo_test.cpp"}, {"src/widget_test.cpp"}};
  src.blobs = {
      {"src/app/foo.cpp", "int foo() { return 1; }"},
      {"tests/unit/foo_test.cpp", "int t() { return 1; }"}, // test directory segment
      {"src/widget_test.cpp", "int w() { return 1; }"},     // test basename
  };

  const auto out = collectNonVendoredSources(src);

  REQUIRE(out.size() == 1);
  REQUIRE(out[0].first == "src/app/foo.cpp");
}

TEST_CASE("DiskFileSource::read skips oversized files (S4)", "[scan][disk_file_source][security]")
{
  auto tree = make_tree("s4_oversize");
  const auto big = tree.root / "huge.h";
  // Create a sparse file of 65 MiB via seekp — stays on disk as sparse,
  // file_size() reports the logical size without reading all bytes.
  {
    std::ofstream f(big, std::ios::binary);
    f.seekp(static_cast<std::streamoff>(65LL * 1024 * 1024 - 1));
    f.put('\0');
  }
  // Small file that must still be readable.
  const auto small = tree.root / "small.h";
  {
    std::ofstream f(small);
    f << "// ok\n";
  }

  archcheck::scan::DiskFileSource src(tree.root);
  // Oversized file returns empty without throwing.
  REQUIRE(src.read("huge.h").empty());
  // Small file is unaffected.
  REQUIRE(src.read("small.h") == "// ok\n");
}
