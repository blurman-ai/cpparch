#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "archcheck/diff/regression_report.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/disk_file_source.h"

namespace
{

namespace fs = std::filesystem;

struct TempDir
{
  fs::path path;
  TempDir()
  {
    const auto tmpl = (fs::temp_directory_path() / "archcheck-itest-XXXXXX").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    REQUIRE(mkdtemp(buf.data()) != nullptr);
    path = fs::path{buf.data()};
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  TempDir(const TempDir &) = delete;
  TempDir &operator=(const TempDir &) = delete;
};

// std::system is fine here — the only interpolated string is `cwd`, which
// comes from mkdtemp(3) (/tmp/archcheck-itest-XXXXXX, no shell metachars).
int runIn(const fs::path &cwd, const std::string &cmd)
{
  const std::string full = "cd '" + cwd.string() + "' && " + cmd + " >/dev/null 2>&1";
  return std::system(full.c_str());
}

void writeFile(const fs::path &p, std::string_view content)
{
  std::ofstream f(p);
  f << content;
}

void initRepo(const fs::path &p)
{
  REQUIRE(runIn(p, "git init -q -b main") == 0);
  REQUIRE(runIn(p, "git config user.email test@example") == 0);
  REQUIRE(runIn(p, "git config user.name test") == 0);
  REQUIRE(runIn(p, "git config commit.gpgsign false") == 0);
}

void commitAll(const fs::path &p, const std::string &msg)
{
  REQUIRE(runIn(p, "git add . && git commit -qm '" + msg + "'") == 0);
}

archcheck::diff::RegressionReport diffRefs(const fs::path &repo, const std::string &baseRef, const std::string &headRef)
{
  archcheck::git::GitError err;
  auto baseTree = archcheck::git::materializeRef(repo, baseRef, err);
  auto headTree = archcheck::git::materializeRef(repo, headRef, err);
  REQUIRE(baseTree.has_value());
  REQUIRE(headTree.has_value());
  const auto baseline = archcheck::graph::buildGraphForPath(baseTree->path());
  const auto current = archcheck::graph::buildGraphForPath(headTree->path());
  return archcheck::diff::buildRegressionReport(baseline.graph, current.graph);
}

// In-memory variant: read blobs through git's object DB, no worktree-add.
// `headRef == kWorktreeRef` falls back to a DiskFileSource on the working tree
// (uncommitted state lives on disk, not in any blob).
archcheck::diff::RegressionReport diffRefsMemory(const fs::path &repo, const std::string &baseRef,
                                                 const std::string &headRef)
{
  archcheck::graph::GraphBuildResult baseline;
  archcheck::graph::GraphBuildResult current;
  if (baseRef == archcheck::git::kWorktreeRef)
  {
    archcheck::scan::DiskFileSource s(repo);
    baseline = archcheck::graph::buildGraphForSource(s);
  }
  else
  {
    archcheck::git::GitObjectFileSource s(repo, baseRef);
    REQUIRE(s.valid());
    baseline = archcheck::graph::buildGraphForSource(s);
  }
  if (headRef == archcheck::git::kWorktreeRef)
  {
    archcheck::scan::DiskFileSource s(repo);
    current = archcheck::graph::buildGraphForSource(s);
  }
  else
  {
    archcheck::git::GitObjectFileSource s(repo, headRef);
    REQUIRE(s.valid());
    current = archcheck::graph::buildGraphForSource(s);
  }
  return archcheck::diff::buildRegressionReport(baseline.graph, current.graph);
}

} // namespace

TEST_CASE("git diff: PR that adds an edge → addedEdges = 1, no regression cycle", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  writeFile(repo.path / "c.h", "// c\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "a.h", "#include \"b.h\"\n#include \"c.h\"\n");
  commitAll(repo.path, "PR adds edge");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.addedEdges.size() == 1);
  REQUIRE(report.addedEdges[0].from == "a.h");
  REQUIRE(report.addedEdges[0].to == "c.h");
  REQUIRE(report.removedEdges.empty());
  REQUIRE(report.grownCycles.empty());
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff: PR that closes a cycle → grownCycles is non-empty", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "#include \"c.h\"\n");
  writeFile(repo.path / "c.h", "// c\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "c.h", "#include \"a.h\"\n");
  commitAll(repo.path, "PR closes cycle");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.grownCycles.size() == 1);
  REQUIRE(report.grownCycles[0].currentSize == 3);
  REQUIRE(report.grownCycles[0].baselineSize == 0);
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff: no-op PR (touch unrelated text) → no regression", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "README.md", "irrelevant\n");
  commitAll(repo.path, "docs only");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE_FALSE(report.hasRegression());
}

TEST_CASE("git diff: single-ref revspec resolves to <ref>..WORKTREE", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  commitAll(repo.path, "baseline");
  // Add an uncommitted edge in the working tree.
  writeFile(repo.path / "c.h", "// c\n");
  writeFile(repo.path / "a.h", "#include \"b.h\"\n#include \"c.h\"\n");

  const auto parsed = archcheck::git::parseRevspec("HEAD");
  REQUIRE(parsed.has_value());
  REQUIRE(parsed->current == archcheck::git::kWorktreeRef);

  const auto report = diffRefs(repo.path, parsed->baseline, parsed->current);
  REQUIRE(report.addedEdges.size() == 1);
  REQUIRE(report.addedEdges[0].from == "a.h");
  REQUIRE(report.addedEdges[0].to == "c.h");
}

TEST_CASE("git diff: findRepoRoot finds the enclosing repo", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "x.h", "// x\n");
  commitAll(repo.path, "init");

  fs::create_directories(repo.path / "deep" / "sub");
  const auto root = archcheck::git::findRepoRoot(repo.path / "deep" / "sub");
  REQUIRE(root.has_value());
  // git resolves symlinks; canonical-compare both sides.
  REQUIRE(fs::canonical(*root) == fs::canonical(repo.path));
}

TEST_CASE("git diff: invalid ref → materializeRef returns nullopt with message", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "x.h", "// x\n");
  commitAll(repo.path, "init");

  archcheck::git::GitError err;
  const auto tree = archcheck::git::materializeRef(repo.path, "no-such-ref", err);
  REQUIRE_FALSE(tree.has_value());
  REQUIRE_FALSE(err.message.empty());
}

TEST_CASE("git diff: changedCppFiles (a..b) lists only C/C++ files", "[diff][git][integration][changed]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "// a\n");
  writeFile(repo.path / "README.md", "v1\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "a.h", "// a v2\n");      // C++ change
  writeFile(repo.path / "README.md", "v2\n");     // docs change — ignored
  writeFile(repo.path / "config.yaml", "x: 1\n"); // new yaml — ignored
  commitAll(repo.path, "mixed change");

  const auto changed = archcheck::git::changedCppFiles(repo.path, "HEAD~1", "HEAD");
  REQUIRE(changed.has_value());
  REQUIRE(changed->size() == 1);
  REQUIRE(changed->front().filename() == "a.h");
}

TEST_CASE("git diff: changedCppFiles (a..b) docs-only PR → empty list", "[diff][git][integration][changed]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "// a\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "README.md", "added\n");
  commitAll(repo.path, "docs only");

  const auto changed = archcheck::git::changedCppFiles(repo.path, "HEAD~1", "HEAD");
  REQUIRE(changed.has_value());
  REQUIRE(changed->empty());
}

TEST_CASE("git diff (memory): added edge → same report as disk path", "[diff][git][integration][memory]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  writeFile(repo.path / "c.h", "// c\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "a.h", "#include \"b.h\"\n#include \"c.h\"\n");
  commitAll(repo.path, "PR adds edge");

  const auto report = diffRefsMemory(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.addedEdges.size() == 1);
  REQUIRE(report.addedEdges[0].from == "a.h");
  REQUIRE(report.addedEdges[0].to == "c.h");
  REQUIRE(report.removedEdges.empty());
  REQUIRE(report.grownCycles.empty());
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff (memory): closed cycle → grownCycles non-empty", "[diff][git][integration][memory]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "#include \"c.h\"\n");
  writeFile(repo.path / "c.h", "// c\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "c.h", "#include \"a.h\"\n");
  commitAll(repo.path, "PR closes cycle");

  const auto report = diffRefsMemory(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.grownCycles.size() == 1);
  REQUIRE(report.grownCycles[0].currentSize == 3);
  REQUIRE(report.grownCycles[0].baselineSize == 0);
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff (memory): no-op PR → no regression", "[diff][git][integration][memory]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "README.md", "irrelevant\n");
  commitAll(repo.path, "docs only");

  const auto report = diffRefsMemory(repo.path, "HEAD~1", "HEAD");
  REQUIRE_FALSE(report.hasRegression());
}

TEST_CASE("git diff (memory): <ref>..WORKTREE picks up uncommitted edge",
          "[diff][git][integration][memory]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "c.h", "// c\n");
  writeFile(repo.path / "a.h", "#include \"b.h\"\n#include \"c.h\"\n");

  const auto report = diffRefsMemory(repo.path, "HEAD", archcheck::git::kWorktreeRef);
  REQUIRE(report.addedEdges.size() == 1);
  REQUIRE(report.addedEdges[0].from == "a.h");
  REQUIRE(report.addedEdges[0].to == "c.h");
}

TEST_CASE("git diff: changedCppFiles (a..WORKTREE) catches uncommitted + untracked C++ files",
          "[diff][git][integration][changed]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "// a\n");
  commitAll(repo.path, "baseline");
  // Modify tracked .h (not committed)
  writeFile(repo.path / "a.h", "// a v2\n");
  // Add a brand-new untracked .cpp
  writeFile(repo.path / "b.cpp", "int x;\n");
  // Plus an untracked README that must NOT show up.
  writeFile(repo.path / "NOTES.md", "x\n");

  const auto changed = archcheck::git::changedCppFiles(repo.path, "HEAD", archcheck::git::kWorktreeRef);
  REQUIRE(changed.has_value());
  REQUIRE(changed->size() == 2);
}
