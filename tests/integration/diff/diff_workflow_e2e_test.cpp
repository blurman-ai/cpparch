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

// One distinctive C source per index: unique helper-call names so the token clone
// detector's candidacy fires (a 2-file toy degenerates — corpus-relative IDF, §3).
std::string modSrc(int i)
{
  const std::string s = std::to_string(i);
  return "extern int helper_" + s +
         "_a(int);\n"
         "extern int helper_" +
         s +
         "_b(int, int);\n"
         "int compute_mod" +
         s +
         "(int x, int y) {\n"
         "  int acc = helper_" +
         s +
         "_a(x);\n"
         "  for (int k = 0; k < y; k++) { acc += helper_" +
         s + "_b(k, x); acc = helper_" + s +
         "_a(acc); }\n"
         "  return acc + helper_" +
         s +
         "_b(y, x);\n"
         "}\n";
}

// A 15-file base — enough corpus for new-clone candidacy to be meaningful.
void distinctiveBase(const fs::path &repo)
{
  initRepo(repo);
  for (int i = 0; i < 15; ++i)
  {
    writeFile(repo / ("mod" + std::to_string(i) + ".c"), modSrc(i));
  }
  commitAll(repo, "baseline");
}

} // namespace

TEST_CASE("e2e --diff: copy-paste a commit introduces fires DRIFT.NEW_CLONE, advisory (#123)", "[diff][e2e][newclone]")
{
  TempDir repo;
  distinctiveBase(repo.path);
  // Append a renamed copy of mod3's distinctive function into mod7 — a clone the
  // commit introduces (its helper_3_* calls make it a candidate against mod3).
  std::string copy = modSrc(3);
  copy.replace(copy.find("compute_mod3"), std::string("compute_mod3").size(), "copied_from_mod3");
  writeFile(repo.path / "mod7.c", modSrc(7) + copy);
  commitAll(repo.path, "copy compute_mod3 into mod7");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.output.find("DRIFT.NEW_CLONE") != std::string::npos);
  REQUIRE(r.output.find("copy-paste introduced") != std::string::npos);
  REQUIRE(r.exitCode == 0); // advisory — never gates
  REQUIRE(r.output.find("gate: ok") != std::string::npos);
}

TEST_CASE("e2e --diff: a commit adding only unique code surfaces no new clone (#123)", "[diff][e2e][newclone]")
{
  TempDir repo;
  distinctiveBase(repo.path);
  writeFile(
      repo.path / "mod7.c",
      modSrc(7) +
          "int unique_only(int p) { int q = p; for (int i = 0; i < p; i++) q ^= (i * 131 + 7); return q % 997; }\n");
  commitAll(repo.path, "add unique code");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.output.find("DRIFT.NEW_CLONE") == std::string::npos);
}

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

TEST_CASE("e2e --diff: unresolvable baseline ref warns on stderr (#124)", "[diff][e2e]")
{
  TempDir repo;
  commitChain(repo.path);

  // A baseline ref that does not resolve: the diff degrades to "whole current vs
  // empty tree" (correct for a real root commit, here a typo). archcheck must
  // surface it instead of silently treating everything as added.
  const auto r = runArchcheck(repo.path, "--diff nonexistentref..HEAD");
  REQUIRE(r.output.find("warning: baseline ref 'nonexistentref' does not resolve") != std::string::npos);
}

TEST_CASE("e2e --diff: bulk import skips graph gating — cycle not gated (#124)", "[diff][e2e]")
{
  TempDir repo;
  commitChain(repo.path);
  // This PR both closes the a->b->c->a cycle (which alone gates → exit 1) AND adds
  // >10000 lines (bulk import). A bulk dump is not the project's authored evolution
  // (vendored / generated / committed-as-is), so the graph checks are skipped and
  // the cycle must NOT block the merge — only slow incremental drift should gate.
  writeFile(repo.path / "c.h", "#include \"a.h\"\n");
  std::string body;
  for (int i = 0; i < 10100; ++i)
    body += "int v" + std::to_string(i) + " = 0;\n";
  writeFile(repo.path / "big.h", body);
  commitAll(repo.path, "bulk import that also closes a cycle");

  const auto r = runArchcheck(repo.path, "--diff HEAD~1..HEAD");
  REQUIRE(r.exitCode == 0); // cycle present but NOT gated, because bulk
  REQUIRE(r.output.find("graph checks:   skipped") != std::string::npos);
  REQUIRE(r.output.find("gate: fail") == std::string::npos);
}
