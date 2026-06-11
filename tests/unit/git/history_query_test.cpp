#include <catch2/catch_test_macros.hpp>

#include "archcheck/git/history_query.h"

namespace
{

TEST_CASE("history_query: parseHistoryOutput with single commit", "[git]")
{
  const std::string output = "abc123\037Initial commit\n"
                             "\n"
                             "5\t3\tsrc/main.cpp\n"
                             "2\t1\tinclude/main.h\n"
                             "\n";

  const auto commits = archcheck::git::parseHistoryOutput(output);

  REQUIRE(commits.size() == 1);
  REQUIRE(commits[0].sha == "abc123");
  REQUIRE(commits[0].subject == "Initial commit");
  REQUIRE(commits[0].files.size() == 2);

  REQUIRE(commits[0].files[0].path == "src/main.cpp");
  REQUIRE(commits[0].files[0].added == 5);
  REQUIRE(commits[0].files[0].deleted == 3);

  REQUIRE(commits[0].files[1].path == "include/main.h");
  REQUIRE(commits[0].files[1].added == 2);
  REQUIRE(commits[0].files[1].deleted == 1);
}

TEST_CASE("history_query: parseHistoryOutput with multiple commits", "[git]")
{
  const std::string output = "aaa111\037First commit\n"
                             "\n"
                             "10\t0\tsrc/foo.cpp\n"
                             "\n"
                             "bbb222\037Second commit\n"
                             "\n"
                             "5\t2\tsrc/foo.cpp\n"
                             "3\t1\tsrc/bar.cpp\n"
                             "\n";

  const auto commits = archcheck::git::parseHistoryOutput(output);

  REQUIRE(commits.size() == 2);

  REQUIRE(commits[0].sha == "aaa111");
  REQUIRE(commits[0].subject == "First commit");
  REQUIRE(commits[0].files.size() == 1);
  REQUIRE(commits[0].files[0].path == "src/foo.cpp");
  REQUIRE(commits[0].files[0].added == 10);
  REQUIRE(commits[0].files[0].deleted == 0);

  REQUIRE(commits[1].sha == "bbb222");
  REQUIRE(commits[1].subject == "Second commit");
  REQUIRE(commits[1].files.size() == 2);
}

TEST_CASE("history_query: parseHistoryOutput skips rename entries", "[git]")
{
  const std::string output = "ccc333\037Rename file\n"
                             "\n"
                             "0\t0\told_name.cpp => new_name.cpp\n"
                             "5\t2\tsrc/other.cpp\n"
                             "\n";

  const auto commits = archcheck::git::parseHistoryOutput(output);

  REQUIRE(commits.size() == 1);
  REQUIRE(commits[0].files.size() == 1);
  // Only the non-rename file should be included
  REQUIRE(commits[0].files[0].path == "src/other.cpp");
  REQUIRE(commits[0].files[0].added == 5);
  REQUIRE(commits[0].files[0].deleted == 2);
}

TEST_CASE("history_query: parseHistoryOutput with empty input", "[git]")
{
  const auto commits = archcheck::git::parseHistoryOutput("");
  REQUIRE(commits.empty());
}

TEST_CASE("history_query: parseHistoryOutput with only whitespace", "[git]")
{
  const std::string output = "\n\n\n";
  const auto commits = archcheck::git::parseHistoryOutput(output);
  REQUIRE(commits.empty());
}

} // namespace
