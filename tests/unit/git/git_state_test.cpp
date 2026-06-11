#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/git/git_exec.h"
#include "archcheck/git/git_state.h"

using archcheck::git::parseRevspec;
using archcheck::git::Revspec;
using archcheck::git::runGit;

TEST_CASE("parseRevspec accepts a..b", "[git][revspec]")
{
  const auto r = parseRevspec("main..HEAD");
  REQUIRE(r.has_value());
  REQUIRE(r->baseline == "main");
  REQUIRE(r->current == "HEAD");
}

TEST_CASE("parseRevspec treats single ref as <ref>..WORKTREE", "[git][revspec]")
{
  const auto r = parseRevspec("HEAD~1");
  REQUIRE(r.has_value());
  REQUIRE(r->baseline == "HEAD~1");
  REQUIRE(r->current == "WORKTREE");
}

TEST_CASE("parseRevspec accepts SHA range", "[git][revspec]")
{
  const auto r = parseRevspec("abc1234..def5678");
  REQUIRE(r.has_value());
  REQUIRE(r->baseline == "abc1234");
  REQUIRE(r->current == "def5678");
}

TEST_CASE("parseRevspec rejects empty input", "[git][revspec]") { REQUIRE_FALSE(parseRevspec("").has_value()); }

TEST_CASE("parseRevspec rejects empty sides", "[git][revspec]")
{
  REQUIRE_FALSE(parseRevspec("..HEAD").has_value());
  REQUIRE_FALSE(parseRevspec("main..").has_value());
}

TEST_CASE("parseRevspec rejects three-dot symmetric form", "[git][revspec]")
{
  // We deliberately only support the two-dot form; '...' is reserved.
  REQUIRE_FALSE(parseRevspec("main...HEAD").has_value());
}

TEST_CASE("runGit hardening: git --version succeeds with hardening flags (S6)", "[git][security]")
{
  // Verify that the hardening flags (-c core.hooksPath=/dev/null etc.) do not
  // break normal git operation.  `git --version` is the safest possible probe.
  const auto r = runGit({"--version"}, {});
  REQUIRE(r.exitCode == 0);
  REQUIRE(r.out.find("git version") != std::string::npos);
}

TEST_CASE("runGit hardening: GIT_CONFIG_NOSYSTEM does not prevent reading commits (S6)", "[git][security]")
{
  // Use the cpparch repo itself as a real repo. findRepoRoot verifies that
  // rev-parse still works after hardening is applied.
  const std::filesystem::path cwd = std::filesystem::current_path();
  const auto root = archcheck::git::findRepoRoot(cwd);
  // If not running inside a git repo (e.g. bare CI), skip gracefully.
  if (!root.has_value())
  {
    SUCCEED("not inside a git repository — skipping");
    return;
  }
  // rev-parse HEAD should still succeed.
  const auto r = runGit({"rev-parse", "HEAD"}, *root);
  REQUIRE(r.exitCode == 0);
  REQUIRE(!r.out.empty());
}
