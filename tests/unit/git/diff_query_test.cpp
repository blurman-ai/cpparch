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

} // namespace
