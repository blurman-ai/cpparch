#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/scan/local_complexity_drift.h"

namespace
{

using archcheck::scan::compareLocalComplexity;

// N sequential braceless `if (x) use(i);` statements => score N.
std::string flatIfs(int n)
{
  std::string body = "void f(int x)\n{\n";
  for (int i = 0; i < n; ++i)
    body += "  if (x) use(" + std::to_string(i) + ");\n";
  return body + "}\n";
}

TEST_CASE("complexity_drift: delta >= 5 yields a soft finding", "[scan][complexity]")
{
  const auto res = compareLocalComplexity(flatIfs(1), flatIfs(6), "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].ruleId == "DRIFT.LOCAL_COMPLEXITY");
  REQUIRE(res.violations[0].file == "a.cpp");
  REQUIRE(res.violations[0].message.find("from 1 to 6") != std::string::npos);
  REQUIRE(res.positiveDelta == 5);
}

TEST_CASE("complexity_drift: harmless append is silent", "[scan][complexity]")
{
  const auto res =
      compareLocalComplexity("void f()\n{\n  run();\n}\n", "void f()\n{\n  run();\n  log();\n}\n", "a.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.positiveDelta == 0);
}

TEST_CASE("complexity_drift: crossing threshold 25 is reported", "[scan][complexity]")
{
  const auto res = compareLocalComplexity(flatIfs(24), flatIfs(26), "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("crossed 25") != std::string::npos);
}

TEST_CASE("complexity_drift: small delta above threshold respects the floor", "[scan][complexity]")
{
  // Δ1–2 tail on already-complex functions is not actionable (#102 verdict).
  REQUIRE(compareLocalComplexity(flatIfs(26), flatIfs(28), "a.cpp").violations.empty());
  const auto res = compareLocalComplexity(flatIfs(26), flatIfs(29), "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("already above 25") != std::string::npos);
}

TEST_CASE("complexity_drift: new function reported only above threshold", "[scan][complexity]")
{
  REQUIRE(compareLocalComplexity("", flatIfs(6), "a.cpp").violations.empty());
  const auto res = compareLocalComplexity("", flatIfs(26), "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("new function 'f'") != std::string::npos);
}

TEST_CASE("complexity_drift: rename counts as new, no growth finding", "[scan][complexity]")
{
  const std::string oldSrc = "void before(int x)\n{\n  if (x) use(0);\n}\n";
  const std::string newSrc = "void after(int x)\n{\n  if (x) { if (x > 1) { use(0); } }\n}\n";
  REQUIRE(compareLocalComplexity(oldSrc, newSrc, "a.cpp").violations.empty());
}

TEST_CASE("complexity_drift: TEST_F macro bodies are ignored", "[scan][complexity]")
{
  const std::string oldSrc = "TEST_F(Suite, Name)\n{\n  if (a) use(0);\n}\n";
  const std::string newSrc = "TEST_F(Suite, Name)\n{\n" + std::string(8, ' ') +
                             "if (a) { if (b) { if (c) { if (d) { if (e) { use(0); } } } } }\n}\n";
  const auto res = compareLocalComplexity(oldSrc, newSrc, "a_test.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.positiveDelta == 0);
}

TEST_CASE("complexity_drift: ambiguous match degrades to low confidence", "[scan][complexity]")
{
  // Two same-name same-arity definitions: nearest-line match, LCX.1/2 suppressed.
  const std::string oldSrc = flatIfs(1) + flatIfs(1);
  const std::string newSrc = flatIfs(1) + flatIfs(30);
  const auto res = compareLocalComplexity(oldSrc, newSrc, "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("crossed") == std::string::npos);
  REQUIRE(res.violations[0].message.find("grew local complexity") != std::string::npos);
}

TEST_CASE("complexity_drift: simplification counts as improvement", "[scan][complexity]")
{
  const auto res = compareLocalComplexity(flatIfs(6), flatIfs(1), "a.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.negativeDelta == -5);
  REQUIRE(res.positiveDelta == 0);
}

} // namespace
