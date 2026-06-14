// Product-level E2E for the canonical PR diff workflow (#075): runs the real
// archcheck binary (ARCHCHECK_BINARY_PATH) on synthesized repos and asserts on
// the final user-facing output and exit code — the contract CI users rely on.

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "support/git_test_repo.h"
#include "support/run_archcheck.h"

namespace
{

namespace fs = std::filesystem;
using namespace archcheck::testsupport;

void commitChain(const fs::path &repo)
{
  initRepo(repo);
  writeFile(repo / "a.h", "#include \"b.h\"\n");
  writeFile(repo / "b.h", "#include \"c.h\"\n");
  writeFile(repo / "c.h", "// c\n");
  commitAll(repo, "baseline");
}

} // namespace

TEST_CASE("e2e --diff: added edge is advisory — exit 0, gate ok", "[diff][e2e]")
{
  TempDir repo;
  commitChain(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n#include \"c.h\"\n");
  commitAll(repo.path, "PR adds edge");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.output.find("added (advisory):") != std::string::npos);
  REQUIRE(r.output.find("gate: ok") != std::string::npos);
}

TEST_CASE("e2e --diff: new cycle gates — exit 1, gate fail", "[diff][e2e]")
{
  TempDir repo;
  commitChain(repo.path);
  writeFile(repo.path / "c.h", "#include \"a.h\"\n");
  commitAll(repo.path, "PR closes cycle");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.exitCode == 1);
  REQUIRE(r.output.find("grown_cycles (gating):") != std::string::npos);
  REQUIRE(r.output.find("gate: fail") != std::string::npos);
}

TEST_CASE("e2e --diff: clean structural diff — zeros, exit 0", "[diff][e2e]")
{
  TempDir repo;
  commitChain(repo.path);
  writeFile(repo.path / "c.h", "// c, comment-only change\n");
  commitAll(repo.path, "harmless change");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.output.find("added_edges:    0") != std::string::npos);
  REQUIRE(r.output.find("gate: ok") != std::string::npos);
}

TEST_CASE("e2e --diff: docs-only PR takes fast path — exit 0", "[diff][e2e]")
{
  TempDir repo;
  commitChain(repo.path);
  writeFile(repo.path / "README.md", "docs\n");
  commitAll(repo.path, "docs-only");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.output.find("no C/C++ files changed; skipping graph build") != std::string::npos);
}

TEST_CASE("e2e --diff --format=json: cycle → gate fail in stable schema", "[diff][e2e][json]")
{
  TempDir repo;
  commitChain(repo.path);
  writeFile(repo.path / "c.h", "#include \"a.h\"\n");
  commitAll(repo.path, "PR closes cycle");

  const auto r = runArchcheck(repo.path, "--diff --format=json HEAD~1..HEAD");
  REQUIRE(r.exitCode == 1);
  REQUIRE(r.output.find("\"version\": 1") != std::string::npos);
  REQUIRE(r.output.find("\"gate\": \"fail\"") != std::string::npos);
  REQUIRE(r.output.find("\"grown_cycles\": [{\"baseline_size\": 0, \"current_size\": 3") != std::string::npos);
}

TEST_CASE("e2e --diff --format=json: docs-only PR → empty report, gate ok", "[diff][e2e][json]")
{
  TempDir repo;
  commitChain(repo.path);
  writeFile(repo.path / "README.md", "docs\n");
  commitAll(repo.path, "docs-only");

  const auto r = runArchcheck(repo.path, "--diff --format=json HEAD~1..HEAD");
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.output.find("\"gate\": \"ok\"") != std::string::npos);
  REQUIRE(r.output.find("\"added_edges\": []") != std::string::npos);
  // Non-bulk diff: advisory zeros are genuine, so the skip marker reads 0.
  REQUIRE(r.output.find("\"complexity_skipped_added_lines\": 0") != std::string::npos);
}

TEST_CASE("e2e --diff --format=json: bulk import exposes skip marker (#117/#124)", "[diff][e2e][json]")
{
  TempDir repo;
  commitChain(repo.path);
  // Exceed the default diff_max_added_lines (10000): the bulk-import gate skips
  // the complexity + new-clone advisories. The JSON must say so, so a consumer
  // can tell this skipped zero from a genuinely clean zero.
  std::string body;
  for (int i = 0; i < 10100; ++i)
    body += "int v" + std::to_string(i) + " = 0;\n";
  writeFile(repo.path / "big.h", body);
  commitAll(repo.path, "bulk import");

  const auto r = runArchcheck(repo.path, "--diff --format=json HEAD~1..HEAD");
  REQUIRE(r.output.find("\"complexity_skipped_added_lines\":") != std::string::npos);
  REQUIRE(r.output.find("\"complexity_skipped_added_lines\": 0,") == std::string::npos);
}
