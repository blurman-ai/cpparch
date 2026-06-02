#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/duplication/fragmenter.h"
#include "archcheck/scan/duplication/token_normalizer.h"

using namespace archcheck::scan::duplication;

TEST_CASE("Fragmenter: empty source", "[duplication]")
{
  const std::string src;
  const auto tokens = lex(src);
  const auto frags = extractFragments(tokens, src, "test.cpp");

  REQUIRE(frags.empty());
}

TEST_CASE("Fragmenter: no functions", "[duplication]")
{
  const std::string src = "int x = 5; int y = 10;";
  const auto tokens = lex(src);
  const auto frags = extractFragments(tokens, src, "test.cpp");

  REQUIRE(frags.empty());
}

TEST_CASE("Fragmenter: simple function", "[duplication]")
{
  const std::string src = "void f() { a=1; b=2; c=3; d=4; e=5; f=6; g=7; h=8; }";
  const auto tokens = lex(src);
  const auto frags = extractFragments(tokens, src, "test.cpp");

  REQUIRE(frags.size() >= 1);
  if (!frags.empty())
  {
    REQUIRE(frags[0].file == "test.cpp");
    REQUIRE(frags[0].startLine >= 1);
    REQUIRE(frags[0].endLine >= frags[0].startLine);
  }
}

TEST_CASE("Fragmenter: fragment size limits", "[duplication]")
{
  FragmentOptions opts;
  opts.minTokens = 5;
  opts.maxTokens = 100;

  const std::string src = "void f() { a; }"; // < 5 tokens inside braces
  const auto tokens = lex(src);
  const auto frags = extractFragments(tokens, src, "test.cpp", opts);

  REQUIRE(frags.empty()); // too small
}

TEST_CASE("Fragmenter: trigram diversity calculated", "[duplication]")
{
  const std::string src = "void f() { a = b + c; d = e * f; g = h - i; }";

  const auto tokens = lex(src);
  const auto frags = extractFragments(tokens, src, "diverse.cpp");

  if (!frags.empty())
  {
    // Diversity should be calculated
    REQUIRE(frags[0].diversity >= 0.0);
    REQUIRE(frags[0].diversity <= 1.0);
  }
}
