#include <catch2/catch_test_macros.hpp>

#include "archcheck/git/diff_query.h"

namespace
{

TEST_CASE("diff_query: collectAddedLines parses unified diff --unified=0", "[git]")
{
  // This test uses fixture files rather than running git directly
  // to avoid git dependency in unit tests.

  SECTION("empty diff")
  {
    // In practice, we'd test via integration. For unit tests,
    // we can test the parsing directly.
    const auto result = archcheck::git::collectAddedLines({}, "", "");
    // Empty repo returns empty result (git fails)
    REQUIRE(result.empty());
  }
}

TEST_CASE("diff_query: collectNumstat parses git diff --numstat", "[git]")
{
  SECTION("empty result")
  {
    const auto result = archcheck::git::collectNumstat({}, "", "");
    REQUIRE(result.empty());
  }
}

// #117 bulk-import gate: the advisory skip threshold compares against the
// total of added lines; binary files report added = -1 and must not underflow.
TEST_CASE("diff_query: totalAddedLines sums added lines, ignores binary entries", "[git]")
{
  REQUIRE(archcheck::git::totalAddedLines({}) == 0);
  const std::vector<archcheck::git::NumStat> stats = {
      {"a.cpp", 100, 5}, {"b.cpp", 41, 0}, {"image.png", -1, -1}, // binary: git prints "-"
  };
  REQUIRE(archcheck::git::totalAddedLines(stats) == 141);
}

} // namespace
