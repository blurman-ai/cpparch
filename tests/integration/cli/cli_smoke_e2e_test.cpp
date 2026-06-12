// Product-level smoke E2E for the CLI surface beyond --diff: check mode,
// formats, baselines, previews, dispatch errors. Runs the real archcheck
// binary — before these tests the CLI layer had no executed-path coverage.

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "support/git_test_repo.h"
#include "support/run_archcheck.h"

namespace
{

namespace fs = std::filesystem;
using namespace archcheck::testsupport;

// Project with one SF.7 violation (using namespace in a header).
void writeDirtyProject(const fs::path &dir)
{
  writeFile(dir / "a.h", "#pragma once\nusing namespace std;\n");
  writeFile(dir / "b.h", "#pragma once\n");
}

} // namespace

TEST_CASE("e2e cli: --help exits 0 and shows the diff workflow", "[cli][e2e]")
{
  TempDir dir;
  const auto r = runArchcheck(dir.path, "--help");
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.output.find("--diff") != std::string::npos);
  REQUIRE(r.output.find("gates: new/grown cycles, new god-headers") != std::string::npos);
}

TEST_CASE("e2e cli: --version prints the product name", "[cli][e2e]")
{
  TempDir dir;
  const auto r = runArchcheck(dir.path, "--version");
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.output.find("archcheck") != std::string::npos);
}

TEST_CASE("e2e cli: unknown argument exits 2 with usage", "[cli][e2e]")
{
  TempDir dir;
  const auto r = runArchcheck(dir.path, "--no-such-flag");
  REQUIRE(r.exitCode == 2);
  REQUIRE(r.output.find("unknown argument") != std::string::npos);
}

TEST_CASE("e2e cli: zero-config check — violation exits 1, clean exits 0", "[cli][e2e]")
{
  TempDir dirty;
  writeDirtyProject(dirty.path);
  const auto bad = runArchcheck(dirty.path, "");
  REQUIRE(bad.exitCode == 1);
  REQUIRE(bad.output.find("[SF.7]") != std::string::npos);

  TempDir clean;
  writeFile(clean.path / "b.h", "#pragma once\n");
  const auto ok = runArchcheck(clean.path, "");
  REQUIRE(ok.exitCode == 0);
}

TEST_CASE("e2e cli: --format json check emits violations schema", "[cli][e2e][json]")
{
  TempDir dir;
  writeDirtyProject(dir.path);
  const auto r = runArchcheck(dir.path, "--format json .");
  REQUIRE(r.exitCode == 1);
  REQUIRE(r.output.find("\"violations\"") != std::string::npos);
  REQUIRE(r.output.find("\"rule\": \"SF.7\"") != std::string::npos);

  const auto bad = runArchcheck(dir.path, "--format yaml .");
  REQUIRE(bad.exitCode == 2);
}

TEST_CASE("e2e cli: --save-baseline freezes debt, --baseline passes on it", "[cli][e2e]")
{
  TempDir dir;
  writeDirtyProject(dir.path);
  REQUIRE(runArchcheck(dir.path, "--save-baseline base.json .").exitCode == 0);
  const auto r = runArchcheck(dir.path, "--baseline base.json .");
  REQUIRE(r.exitCode == 0);
}

TEST_CASE("e2e cli: graph baseline save + drift gate round-trip", "[cli][e2e]")
{
  TempDir dir;
  writeDirtyProject(dir.path);
  REQUIRE(runArchcheck(dir.path, "--save-graph-baseline graph.json .").exitCode == 0);
  REQUIRE(runArchcheck(dir.path, "--drift-baseline graph.json .").exitCode == 0);
}

TEST_CASE("e2e cli: --scan and --graph previews exit 0", "[cli][e2e]")
{
  TempDir dir;
  writeDirtyProject(dir.path);
  REQUIRE(runArchcheck(dir.path, "--scan .").exitCode == 0);
  REQUIRE(runArchcheck(dir.path, "--graph .").exitCode == 0);
}

TEST_CASE("e2e cli: --duplication and --history are advisory — always exit 0", "[cli][e2e]")
{
  TempDir dir;
  initRepo(dir.path);
  writeDirtyProject(dir.path);
  commitAll(dir.path, "init");
  REQUIRE(runArchcheck(dir.path, "--duplication .").exitCode == 0);
  REQUIRE(runArchcheck(dir.path, "--history .").exitCode == 0);
}

TEST_CASE("e2e cli: --diff dispatch errors exit 2", "[cli][e2e]")
{
  TempDir dir;
  REQUIRE(runArchcheck(dir.path, "--diff").exitCode == 2); // no revspec
  REQUIRE(runArchcheck(dir.path, "--diff --diff-mode=floppy HEAD").exitCode == 2);
  REQUIRE(runArchcheck(dir.path, "--diff --format=yaml HEAD").exitCode == 2);
  REQUIRE(runArchcheck(dir.path, "--diff HEAD~1..HEAD").exitCode == 2); // not a git repo
}
