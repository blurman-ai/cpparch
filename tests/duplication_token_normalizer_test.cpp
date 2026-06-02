#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/duplication/token_normalizer.h"

using namespace archcheck::scan::duplication;

TEST_CASE("Token normalizer: simple identifier", "[duplication]")
{
  const std::string src = "int x = 5;";
  const auto tokens = lex(src);

  REQUIRE(tokens.size() == 5);
  REQUIRE(tokens[0].sym == "int");
  REQUIRE(tokens[0].line == 1);
  REQUIRE(tokens[1].sym == "id");
  REQUIRE(tokens[2].sym == "=");
  REQUIRE(tokens[3].sym == "lit");
  REQUIRE(tokens[4].sym == ";");
}

TEST_CASE("Token normalizer: comment handling", "[duplication]")
{
  const std::string src = "int x; // comment\nint y;";
  const auto tokens = lex(src);

  REQUIRE(tokens.size() == 6);
  REQUIRE(tokens[0].line == 1);
  REQUIRE(tokens[3].line == 2); // "int" on line 2
  REQUIRE(tokens[5].line == 2); // ";" on line 2
}

TEST_CASE("Token normalizer: string literal", "[duplication]")
{
  const std::string src = R"(const char* msg = "hello";)";
  const auto tokens = lex(src);

  bool found = false;
  for (const auto &t : tokens)
  {
    if (t.sym == "lit" && t.raw == R"("hello")")
    {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("Token normalizer: keepCalls=true preserves function names", "[duplication]")
{
  const std::string src = "void func() { call(x); }";
  const auto tokens = lex(src, true);

  bool foundCall = false;
  for (const auto &t : tokens)
  {
    if (t.sym == "call") // should be preserved, not "id"
    {
      foundCall = true;
      break;
    }
  }
  REQUIRE(foundCall);
}

TEST_CASE("Token normalizer: keepCalls=false normalizes all identifiers", "[duplication]")
{
  const std::string src = "void func() { call(x); }";
  const auto tokens = lex(src, false);

  bool foundCall = false;
  for (const auto &t : tokens)
  {
    if (t.sym == "call")
    {
      foundCall = true;
      break;
    }
  }
  REQUIRE(!foundCall);
}
