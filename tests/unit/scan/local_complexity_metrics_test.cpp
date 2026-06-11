#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/local_complexity_metrics.h"

namespace
{

// Score of the first (single) function in the source.
int scoreOf(const std::string &src)
{
  const auto fns = archcheck::scan::computeFileComplexity(src);
  REQUIRE(fns.size() == 1);
  return fns[0].score;
}

// Test matrix from #101 — counter-examples from the scorer review are
// mandatory regressions (experiments/local_complexity_drift/review_repros/).

TEST_CASE("complexity: flat switch with 8 cases scores 1", "[scan][complexity]")
{
  REQUIRE(scoreOf("int dispatch(int t)\n{\n  switch (t)\n  {\n  case 1: return 1;\n  case 2: return 2;\n"
                  "  case 3: return 3;\n  case 4: return 4;\n  case 5: return 5;\n  case 6: return 6;\n"
                  "  case 7: return 7;\n  default: return 0;\n  }\n}\n") == 1);
}

TEST_CASE("complexity: nested ifs accumulate nesting 1+2+3", "[scan][complexity]")
{
  REQUIRE(scoreOf("void f(int a)\n{\n  if (a) {\n    if (a > 1) {\n      if (a > 2) { use(a); }\n    }\n  }\n}\n") ==
          6);
}

TEST_CASE("complexity: else-if chain of five conditions scores 5", "[scan][complexity]")
{
  REQUIRE(scoreOf("int m(int c)\n{\n  if (c == 1) return 10;\n  else if (c == 2) return 20;\n"
                  "  else if (c == 3) return 30;\n  else if (c == 4) return 40;\n"
                  "  else if (c == 5) return 50;\n  return 0;\n}\n") == 5);
}

TEST_CASE("complexity: logical series count once, operator change adds one", "[scan][complexity]")
{
  REQUIRE(scoreOf("bool h(bool a, bool b, bool c)\n{\n  return a && b && c;\n}\n") == 1);
  REQUIRE(scoreOf("bool h(bool a, bool b, bool c)\n{\n  return a && b || c;\n}\n") == 2);
}

TEST_CASE("complexity: rvalue references are not logical operators", "[scan][complexity]")
{
  REQUIRE(scoreOf("void take(Item item)\n{\n  Item&& ref = std::move(item);\n  auto&& any = make();\n"
                  "  sink(std::forward<Item>(ref), any);\n}\n") == 0);
}

TEST_CASE("complexity: aligned argument continuations score 0", "[scan][complexity]")
{
  REQUIRE(scoreOf("int wire(int a, int b)\n{\n  configure(a,\n            b,\n            kFlag,\n"
                  "            nullptr);\n  return a + b;\n}\n") == 0);
}

TEST_CASE("complexity: do-while costs one increment", "[scan][complexity]")
{
  REQUIRE(scoreOf("void pump()\n{\n  do {\n    step();\n  } while (more());\n}\n") == 1);
}

TEST_CASE("complexity: lambda body adds a nesting level", "[scan][complexity]")
{
  REQUIRE(scoreOf("int visit(int v)\n{\n  auto f = [](int x) {\n    if (x) return 1;\n    return 0;\n  };\n"
                  "  return f(v);\n}\n") == 2);
}

TEST_CASE("complexity: keywords inside strings, comments and raw strings score 0", "[scan][complexity]")
{
  REQUIRE(scoreOf("const char* text()\n{\n  // if (x) while (y) for (;;)\n  /* switch (z) { case 1: } */\n"
                  "  return R\"(if (a && b) { do; })\";\n}\n") == 0);
}

TEST_CASE("complexity: goto adds one, early return/break/continue are free", "[scan][complexity]")
{
  // if = 1, goto = 1, for = 1; break/continue/return = 0.
  REQUIRE(scoreOf("void f(bool x)\n{\n  if (x) goto done;\n  for (;;) {\n    break;\n  }\ndone:\n  return;\n}\n") == 3);
}

TEST_CASE("complexity: init-list data table scores 0", "[scan][complexity]")
{
  REQUIRE(scoreOf("Table build()\n{\n  Table t = {\n      {1, \"one\"},\n      {2, \"two\"},\n      {3, \"three\"},\n"
                  "  };\n  return t;\n}\n") == 0);
}

TEST_CASE("complexity: braceless for+if nests the if", "[scan][complexity]")
{
  // for = 1, if = 1 + nesting(1) = 2.
  REQUIRE(scoreOf("void g(int n)\n{\n  for (int i = 0; i < n; ++i)\n    if (i % 2)\n      use(i);\n}\n") == 3);
}

TEST_CASE("complexity: preprocessor branches score 0 by default", "[scan][complexity]")
{
  REQUIRE(scoreOf("int g(int x)\n{\n#if defined(FOO)\n  return x;\n#else\n  return -x;\n#endif\n}\n") == 0);
}

TEST_CASE("complexity: multi-line directive continuation is stripped", "[scan][complexity]")
{
  REQUIRE(scoreOf("int g(int x)\n{\n#define CHECK(a) \\\n  if (a) { trap(); }\n  return x;\n}\n") == 0);
}

TEST_CASE("complexity: meaningful loc counts token lines", "[scan][complexity]")
{
  const auto fns =
      archcheck::scan::computeFileComplexity("int f()\n{\n  int a = 1;\n\n  // comment only\n  return a;\n}\n");
  REQUIRE(fns.size() == 1);
  REQUIRE(fns[0].meaningfulLoc == 2);
}

} // namespace
