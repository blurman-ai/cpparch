#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>

#include "archcheck/scan/local_complexity_drift.h"
#include "archcheck/scan/source_snapshot.h"

namespace
{

using archcheck::scan::compareLocalComplexity;
using archcheck::scan::detectLocalComplexityDrift;
using archcheck::scan::SourceSnapshot;

struct MapFileSource final : archcheck::scan::FileSource
{
  std::map<std::string, std::string> files;

  std::vector<archcheck::scan::ProjectFile> list() override
  {
    std::vector<archcheck::scan::ProjectFile> out;
    for (const auto &[path, content] : files)
      out.push_back({path});
    return out;
  }
  std::string read(const std::string &path) override
  {
    const auto it = files.find(path);
    return it == files.end() ? std::string{} : it->second;
  }
};

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

// Corpus FP class "deletion-shift" (#109): equal-arity overloads told apart by
// parameter spelling must match exactly even when line numbers shift a lot.
TEST_CASE("complexity_drift: equal-arity overloads match by parameter fingerprint", "[scan][complexity]")
{
  const std::string tiny = "void f(const Str &a, const Str &b)\n{\n  use(a);\n}\n";
  std::string big = "void f(const Int &a, const Int &b)\n{\n";
  for (int i = 0; i < 30; ++i)
    big += "  if (a) use(" + std::to_string(i) + ");\n";
  big += "}\n";
  // Old: tiny(str) right above big(int). New: 40 lines deleted before both —
  // nearest-line would now pair big(int) with tiny(str) and report 0 -> 30.
  const std::string pad(40, '\n');
  const auto res = compareLocalComplexity(pad + tiny + big, tiny + big, "a.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.positiveDelta == 0);
}

// Corpus FP class "arity-change-new-func" (#109): a signature change must not
// resurface the whole carried-over body as a "new function" finding.
TEST_CASE("complexity_drift: arity change is not a new function", "[scan][complexity]")
{
  std::string oldSrc = "void f(int x)\n{\n";
  std::string newSrc = "void f(int x, bool flip)\n{\n";
  for (int i = 0; i < 30; ++i)
  {
    oldSrc += "  if (x) use(" + std::to_string(i) + ");\n";
    newSrc += "  if (x) use(" + std::to_string(i) + ");\n";
  }
  const auto res = compareLocalComplexity(oldSrc + "}\n", newSrc + "}\n", "a.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.positiveDelta == 0);
}

TEST_CASE("complexity_drift: arity change with real growth keeps the soft signal", "[scan][complexity]")
{
  std::string newSrc = "void f(int x, bool flip)\n{\n";
  for (int i = 0; i < 8; ++i)
    newSrc += "  if (x) use(" + std::to_string(i) + ");\n";
  const auto res = compareLocalComplexity(flatIfs(2), newSrc + "}\n", "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("grew local complexity from 2 to 8") != std::string::npos);
}

// Corpus FP "unparseable parent" (#109): ArduPilot 924a3e3860^ had an unclosed
// brace, so the parent span was lost and the child resurfaced as new.
TEST_CASE("complexity_drift: structurally unbalanced side mutes the file", "[scan][complexity]")
{
  const std::string broken = "void f(int x)\n{\n  if (x) {\n    use(0);\n";
  REQUIRE(compareLocalComplexity(broken, flatIfs(30), "a.cpp").violations.empty());
  REQUIRE(compareLocalComplexity(flatIfs(1), broken, "a.cpp").violations.empty());
}

TEST_CASE("complexity_drift: stray extra close brace does not mute the file", "[scan][complexity]")
{
  // Forward discovery self-heals after a stray `}` (#109 125jdavis case).
  const std::string stray = "}\n";
  const auto res = compareLocalComplexity(stray + flatIfs(1), stray + flatIfs(6), "a.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("from 1 to 6") != std::string::npos);
}

// #109 gromacs case: wrapping definitions in `namespace gmx {` must not
// resurface every function as new.
TEST_CASE("complexity_drift: namespace move is not a new function", "[scan][complexity]")
{
  std::string body;
  for (int i = 0; i < 30; ++i)
    body += "  if (x) use(" + std::to_string(i) + ");\n";
  const std::string oldSrc = "void rd(int x)\n{\n" + body + "}\n";
  const std::string newSrc = "namespace gmx {\nvoid rd(int x)\n{\n" + body + "}\n}\n";
  const auto res = compareLocalComplexity(oldSrc, newSrc, "a.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.positiveDelta == 0);
}

// #109 foundationdb case: moving a function body to another file of the same
// diff must not resurface it as from-zero growth — complexity relocated.
TEST_CASE("complexity_drift: cross-file move is not growth", "[scan][complexity]")
{
  std::string body;
  for (int i = 0; i < 30; ++i)
    body += "  if (x) use(" + std::to_string(i) + ");\n";
  const std::string impl = "void peek(int x)\n{\n" + body + "}\n";
  const std::string stub = "void peek(int x)\n{\n  fwd(x);\n}\n";
  MapFileSource olds, news;
  olds.files = {{"a.cpp", impl}, {"b.cpp", stub}};
  news.files = {{"a.cpp", stub}, {"b.cpp", impl}};
  const auto res =
      detectLocalComplexityDrift(SourceSnapshot::read(olds), SourceSnapshot::read(news), {"a.cpp", "b.cpp"});
  REQUIRE(res.violations.empty());
  // The aggregate deltas still reflect both sides of the move.
  REQUIRE(res.positiveDelta == 30);
  REQUIRE(res.negativeDelta == -30);
}

// #109 minsky counter-example: implementing a stub overload while DELETING the
// equal-arity old overload is a migration — the sibling must not mask the
// deletion. Keeping both overloads is duplication and must still fire.
TEST_CASE("complexity_drift: overload migration is silent, duplication fires", "[scan][complexity]")
{
  std::string body;
  for (int i = 0; i < 17; ++i)
    body += "  if (x) use(" + std::to_string(i) + ");\n";
  const std::string oldImpl = "void draw(Cairo* c)\n{\n" + body + "}\n";
  const std::string stub = "void draw(const Shim& s)\n{\n  fwd(s);\n}\n";
  const std::string newImpl = "void draw(const Shim& s)\n{\n" + body + "}\n";
  MapFileSource olds, migrated, duplicated;
  olds.files = {{"g.cpp", oldImpl + stub}};
  migrated.files = {{"g.cpp", newImpl}};
  duplicated.files = {{"g.cpp", oldImpl + newImpl}};
  REQUIRE(detectLocalComplexityDrift(SourceSnapshot::read(olds), SourceSnapshot::read(migrated), {"g.cpp"})
              .violations.empty());
  const auto dup = detectLocalComplexityDrift(SourceSnapshot::read(olds), SourceSnapshot::read(duplicated), {"g.cpp"});
  REQUIRE(dup.violations.size() == 1);
  REQUIRE(dup.violations[0].message.find("from 0 to 17") != std::string::npos);
}

TEST_CASE("complexity_drift: genuinely new function is not eaten by the move pool", "[scan][complexity]")
{
  std::string body;
  for (int i = 0; i < 30; ++i)
    body += "  if (x) use(" + std::to_string(i) + ");\n";
  MapFileSource olds, news;
  olds.files = {{"a.cpp", ""}};
  news.files = {{"a.cpp", "void peek(int x)\n{\n" + body + "}\n"}};
  const auto res = detectLocalComplexityDrift(SourceSnapshot::read(olds), SourceSnapshot::read(news), {"a.cpp"});
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("new function 'peek'") != std::string::npos);
}

TEST_CASE("complexity_drift: simplification counts as improvement", "[scan][complexity]")
{
  const auto res = compareLocalComplexity(flatIfs(6), flatIfs(1), "a.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.negativeDelta == -5);
  REQUIRE(res.positiveDelta == 0);
}

} // namespace
