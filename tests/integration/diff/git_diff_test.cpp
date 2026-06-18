#include <unistd.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "archcheck/config/config.h"
#include "archcheck/diff/regression_report.h"
#include "archcheck/git/diff_query.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/git/history_query.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/god_file_growth.h"
#include "archcheck/scan/local_complexity_drift.h"
#include "archcheck/scan/satd_scan.h"
#include "archcheck/scan/source_snapshot.h"
#include "archcheck/scan/test_co_evolution.h"

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

TEST_CASE("git diff: first cross-area dependency is reported as a new area pair", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  fs::create_directories(repo.path / "src");
  fs::create_directories(repo.path / "app");
  writeFile(repo.path / "src" / "core.h", "// core\n");
  writeFile(repo.path / "src" / "util.h", "#include \"core.h\"\n");
  commitAll(repo.path, "baseline");
  // app -> src is a real cross-area link. (tests/ -> src is intentionally NOT a
  // signal: test code is excluded from the graph, #070.)
  writeFile(repo.path / "app" / "widget.h", "#include \"../src/core.h\"\n");
  commitAll(repo.path, "PR adds app -> src link");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.newCrossAreaDependencies.size() == 1);
  REQUIRE(report.newCrossAreaDependencies[0].fromArea == "app");
  REQUIRE(report.newCrossAreaDependencies[0].toArea == "src");
  REQUIRE(report.newCrossAreaDependencies[0].edgeCount == 1);
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff: test code is excluded from the graph (#070)", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  fs::create_directories(repo.path / "src");
  fs::create_directories(repo.path / "tests");
  writeFile(repo.path / "src" / "core.h", "// core\n");
  writeFile(repo.path / "src" / "util.h", "#include \"core.h\"\n");
  commitAll(repo.path, "baseline");
  // A test starting to depend on src is NOT architecture drift — test code is
  // not checked for architecture, so nothing must be reported.
  writeFile(repo.path / "tests" / "core_test.h", "#include \"../src/core.h\"\n");
  commitAll(repo.path, "PR adds tests -> src link");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.newCrossAreaDependencies.empty());
  REQUIRE_FALSE(report.hasRegression());
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

// #109 skyrim case: a renamed file must surface BOTH paths (old as deleted,
// new as added) so the LCX move pool sees the disappearing side.
TEST_CASE("git diff: changedCppFiles reports a rename as both paths", "[diff][git][integration][changed]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "old_name.cpp", "void f(int x)\n{\n  if (x) use(1);\n}\n");
  commitAll(repo.path, "baseline");
  REQUIRE(runIn(repo.path, "git mv old_name.cpp new_name.cpp") == 0);
  commitAll(repo.path, "rename");

  const auto changed = archcheck::git::changedCppFiles(repo.path, "HEAD~1", "HEAD");
  REQUIRE(changed.has_value());
  REQUIRE(changed->size() == 2);
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

TEST_CASE("git diff (memory): <ref>..WORKTREE picks up uncommitted edge", "[diff][git][integration][memory]")
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

// Build A→B (a->c) and A→C (a->d), merge B into C with a manual conflict
// resolution that keeps both edges. HEAD ends up as the merge commit M.
// Tags `A` and `C` are left on the corresponding commits.
void buildMergeRepo(const fs::path &p)
{
  initRepo(p);
  writeFile(p / "a.h", "// a\n");
  writeFile(p / "c.h", "// c\n");
  writeFile(p / "d.h", "// d\n");
  commitAll(p, "baseline");
  REQUIRE(runIn(p, "git tag A") == 0);
  REQUIRE(runIn(p, "git checkout -qb feat-b") == 0);
  writeFile(p / "a.h", "#include \"c.h\"\n");
  commitAll(p, "feat-b adds a->c");
  REQUIRE(runIn(p, "git checkout -q A && git checkout -qb feat-c") == 0);
  writeFile(p / "a.h", "#include \"d.h\"\n");
  commitAll(p, "feat-c adds a->d");
  REQUIRE(runIn(p, "git tag C") == 0);
  // Conflict on a.h is expected — resolve to the union, then commit to seal.
  runIn(p, "git merge --no-ff --no-edit feat-b");
  writeFile(p / "a.h", "#include \"c.h\"\n#include \"d.h\"\n");
  REQUIRE(runIn(p, "git add a.h && git commit -qm merge") == 0);
  REQUIRE(runIn(p, "git rev-parse -q --verify HEAD^2") == 0);
}

TEST_CASE("git diff: merge-commit HEAD → A..M sees union of edges from both parents", "[diff][git][integration][merge]")
{
  TempDir repo;
  buildMergeRepo(repo.path);

  const auto fromA = diffRefs(repo.path, "A", "HEAD");
  REQUIRE(fromA.addedEdges.size() == 2);
  const auto hasEdge = [&](std::string_view to)
  {
    return std::any_of(fromA.addedEdges.begin(), fromA.addedEdges.end(),
                       [&](const auto &e) { return e.from == "a.h" && e.to == to; });
  };
  REQUIRE(hasEdge("c.h"));
  REQUIRE(hasEdge("d.h"));
  REQUIRE(fromA.removedEdges.empty());

  // C..M: only the edge feat-b brought in (a->c). a->d was already in C.
  const auto fromC = diffRefs(repo.path, "C", "HEAD");
  REQUIRE(fromC.addedEdges.size() == 1);
  REQUIRE(fromC.addedEdges[0].from == "a.h");
  REQUIRE(fromC.addedEdges[0].to == "c.h");
  REQUIRE(fromC.removedEdges.empty());
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

// --- metric regression tests (git-based) ---

TEST_CASE("git diff: PR deepens include chain → chainLengthGrown detected", "[diff][git][integration][metrics]")
{
  TempDir repo;
  initRepo(repo.path);
  // baseline: a -> b (depth 1)
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  commitAll(repo.path, "baseline");
  // PR adds b -> c -> d, max depth becomes 3
  writeFile(repo.path / "b.h", "#include \"c.h\"\n");
  writeFile(repo.path / "c.h", "#include \"d.h\"\n");
  writeFile(repo.path / "d.h", "// d\n");
  commitAll(repo.path, "PR deepens chain");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.chainLengthGrown.has_value());
  REQUIRE(report.chainLengthGrown->current > report.chainLengthGrown->baseline);
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff: PR creates god-header → newGodHeaders non-empty", "[diff][git][integration][metrics]")
{
  TempDir repo;
  initRepo(repo.path);
  // baseline: hub.h with 2 includers (below threshold 3)
  writeFile(repo.path / "hub.h", "// hub\n");
  writeFile(repo.path / "c0.h", "#include \"hub.h\"\n");
  writeFile(repo.path / "c1.h", "#include \"hub.h\"\n");
  commitAll(repo.path, "baseline");
  // PR adds a third includer → crosses threshold
  writeFile(repo.path / "c2.h", "#include \"hub.h\"\n");
  commitAll(repo.path, "PR adds third includer");

  archcheck::git::GitError err;
  auto baseTree = archcheck::git::materializeRef(repo.path, "HEAD~1", err);
  auto headTree = archcheck::git::materializeRef(repo.path, "HEAD", err);
  REQUIRE(baseTree.has_value());
  REQUIRE(headTree.has_value());
  const auto baseline = archcheck::graph::buildGraphForPath(baseTree->path());
  const auto current = archcheck::graph::buildGraphForPath(headTree->path());

  archcheck::diff::MetricThresholds t;
  t.godHeaderFanIn = 2; // threshold = 2; 3 includers crosses it
  const auto report = archcheck::diff::buildRegressionReport(baseline.graph, current.graph, t);
  REQUIRE(report.newGodHeaders.size() == 1);
  REQUIRE(report.newGodHeaders[0] == "hub.h");
  REQUIRE(report.hasRegression());
}

TEST_CASE("god-header threshold default is one contract for check and --diff", "[diff][config][metrics]")
{
  REQUIRE(archcheck::diff::MetricThresholds{}.godHeaderFanIn == archcheck::config::Config{}.thresholds.godHeaderFanIn);
}

TEST_CASE("git diff: PR increases NCCD → nccdDelta detected", "[diff][git][integration][metrics]")
{
  TempDir repo;
  initRepo(repo.path);
  // baseline: three isolated headers
  writeFile(repo.path / "a.h", "// a\n");
  writeFile(repo.path / "b.h", "// b\n");
  writeFile(repo.path / "c.h", "// c\n");
  commitAll(repo.path, "baseline");
  // PR densely connects them: a->b, a->c, b->c
  writeFile(repo.path / "a.h", "#include \"b.h\"\n#include \"c.h\"\n");
  writeFile(repo.path / "b.h", "#include \"c.h\"\n");
  commitAll(repo.path, "PR densifies graph");

  const auto report = diffRefs(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.nccdDelta.has_value());
  REQUIRE(*report.nccdDelta > 0.0);
  REQUIRE(report.hasRegression());
}

TEST_CASE("git diff (memory): chain length regression detected via GitObjectFileSource",
          "[diff][git][integration][metrics][memory]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "b.h", "#include \"c.h\"\n");
  writeFile(repo.path / "c.h", "#include \"d.h\"\n");
  writeFile(repo.path / "d.h", "// d\n");
  commitAll(repo.path, "PR deepens chain");

  const auto report = diffRefsMemory(repo.path, "HEAD~1", "HEAD");
  REQUIRE(report.chainLengthGrown.has_value());
  REQUIRE(report.chainLengthGrown->current > report.chainLengthGrown->baseline);
  REQUIRE(report.hasRegression());
}

TEST_CASE("satd_scan: collectAddedLines finds TODO in added lines", "[satd][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "code.cpp", "int x = 1;\n");
  commitAll(repo.path, "baseline");
  writeFile(repo.path / "code.cpp", "int x = 1;\n// TODO: refactor this\nint y = 2;\n");
  commitAll(repo.path, "adds TODO");

  const auto added = archcheck::git::collectAddedLines(repo.path, "HEAD~1", "HEAD");
  REQUIRE(added.size() == 2); // The TODO line and the y=2 line

  const auto satdViolations = archcheck::scan::detectSatdMarkers(added);
  REQUIRE(satdViolations.size() == 1);
  REQUIRE(satdViolations[0].ruleId == "SATD.1");
  REQUIRE(satdViolations[0].file == "code.cpp");
  REQUIRE(satdViolations[0].line > 0);
}

TEST_CASE("test_co_evolution: production change without test update triggers detection",
          "[test_co_evolution][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  fs::create_directories(repo.path / "src");
  fs::create_directories(repo.path / "tests");
  writeFile(repo.path / "src" / "main.cpp", "int main() { return 0; }\n");
  writeFile(repo.path / "tests" / "test_main.cpp", "void test_main() { }\n");
  commitAll(repo.path, "baseline");

  // Change production file by >80 lines, leave tests unchanged
  std::string largeChange;
  for (int i = 0; i < 85; ++i)
    largeChange += "x = " + std::to_string(i) + ";\n";
  writeFile(repo.path / "src" / "main.cpp", largeChange);
  commitAll(repo.path, "large prod change, no test update");

  const auto numstat = archcheck::git::collectNumstat(repo.path, "HEAD~1", "HEAD");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "TEST.1.prod_changed_tests_silent");
}

TEST_CASE("history_query: end-to-end with synthesized git history", "[git][history]")
{
  // Instead of querying a real git repo (which requires git to output the exact format),
  // we test the detector by synthesizing commit history that looks like what
  // queryCommitHistory would return, then verifying the detector fires correctly.
  // The parseHistoryOutput parser is tested separately in unit tests.

  // Synthesize history: 6 commits with steady growth
  std::vector<archcheck::git::CommitStats> history;

  // Initial commit with 100 lines
  archcheck::git::CommitStats c0;
  c0.sha = "abc000";
  c0.subject = "Initial file";
  archcheck::git::FileChange f0;
  f0.path = "src/growing.cpp";
  f0.added = 100;
  f0.deleted = 0;
  c0.files.push_back(f0);
  history.push_back(c0);

  // 5 growth commits, each adding 50 lines with 10 deleted (net +40 per commit)
  for (int i = 1; i <= 5; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "src/growing.cpp";
    fc.added = 50;
    fc.deleted = 10;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  // Current LOC: 100 (initial) + 5 * 40 (net per growth commit) = 300
  // Create additional small files to make P75 calculation meaningful
  std::map<std::string, std::int32_t> currentLoc;
  currentLoc["src/growing.cpp"] = 300; // Net +200 from initial
  currentLoc["src/small.cpp"] = 50;
  currentLoc["src/tiny.cpp"] = 40;
  currentLoc["src/medium.cpp"] = 200;

  // P75 of [40, 50, 200, 300] = [40, 50, 200, 300], idx = ceil(4*0.75)-1 = 2, P75=200
  // growing.cpp: 300 >= 200 ✓
  // net growth: 200 >= +30%*300 = 90 ✓ (OR >= 300)
  // consecutive: 5 >= 5 ✓
  // no shrink ✓

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SIZE.1.god_file_growth");
  REQUIRE(violations[0].file == "src/growing.cpp");
  REQUIRE(violations[0].line == 0);
}

namespace
{

// Read both sides of `base..head` through the object DB and run the local
// complexity comparison over the changed C/C++ files (#101).
archcheck::scan::ComplexityDriftResult complexityDrift(const fs::path &repo, const std::string &baseRef,
                                                       const std::string &headRef)
{
  const auto changed = archcheck::git::changedCppFiles(repo, baseRef, headRef);
  REQUIRE(changed.has_value());
  archcheck::git::GitObjectFileSource oldSource(repo, baseRef);
  archcheck::git::GitObjectFileSource newSource(repo, headRef);
  REQUIRE(oldSource.valid());
  REQUIRE(newSource.valid());
  return archcheck::scan::detectLocalComplexityDrift(archcheck::scan::SourceSnapshot::read(oldSource),
                                                     archcheck::scan::SourceSnapshot::read(newSource), *changed);
}

} // namespace

TEST_CASE("git diff: local complexity growth in a changed function is reported", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "logic.cpp", "int pick(int a)\n{\n  if (a > 0) return 1;\n  return 0;\n}\n");
  commitAll(repo.path, "base");
  writeFile(repo.path / "logic.cpp", "int pick(int a)\n{\n  if (a > 0) {\n    if (a > 1) {\n      if (a > 2) {\n"
                                     "        if (a > 3) { return 4; }\n      }\n    }\n  }\n  return 0;\n}\n");
  commitAll(repo.path, "growth");

  const auto drift = complexityDrift(repo.path, "HEAD~1", "HEAD");
  REQUIRE(drift.violations.size() == 1);
  REQUIRE(drift.violations[0].ruleId == "DRIFT.LOCAL_COMPLEXITY");
  REQUIRE(drift.violations[0].file == "logic.cpp");
  REQUIRE(drift.violations[0].message.find("'pick'") != std::string::npos);
  REQUIRE(drift.violations[0].message.find("from 1 to 10") != std::string::npos);
}

TEST_CASE("git diff: harmless change produces no complexity finding", "[diff][git][integration]")
{
  TempDir repo;
  initRepo(repo.path);
  writeFile(repo.path / "logic.cpp", "void act()\n{\n  run();\n}\n");
  commitAll(repo.path, "base");
  writeFile(repo.path / "logic.cpp", "void act()\n{\n  run();\n  log();\n}\n");
  commitAll(repo.path, "append");

  const auto drift = complexityDrift(repo.path, "HEAD~1", "HEAD");
  REQUIRE(drift.violations.empty());
  REQUIRE(drift.positiveDelta == 0);
}
