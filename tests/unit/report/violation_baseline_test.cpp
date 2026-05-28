#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "archcheck/report/violation_baseline.h"

using archcheck::report::BaselineError;
using archcheck::report::ViolationBaseline;
using archcheck::report::filterNew;
using archcheck::report::loadBaseline;
using archcheck::report::saveBaseline;
using archcheck::rules::Violation;
using archcheck::rules::ViolationList;

namespace fs = std::filesystem;

namespace
{

struct TempFile
{
  fs::path path;
  explicit TempFile(const std::string &name) : path(fs::temp_directory_path() / name) {}
  ~TempFile() { fs::remove(path); }
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
};

const Violation kSF7{"SF.7", "include/bar.h", 12, "using namespace std"};
const Violation kSF9{"SF.9", "src/foo.h", 0, "cycle: a → b → a"};

} // namespace

TEST_CASE("baseline: save + load round-trip", "[baseline]")
{
  TempFile tmp("archcheck-baseline-roundtrip.json");
  ViolationBaseline b;
  b.known = {kSF7, kSF9};
  saveBaseline(b, tmp.path);

  const auto loaded = loadBaseline(tmp.path);
  REQUIRE(loaded.known.size() == 2);
  REQUIRE(loaded.known[0].ruleId == kSF7.ruleId);
  REQUIRE(loaded.known[0].file == kSF7.file);
  REQUIRE(loaded.known[0].line == kSF7.line);
  REQUIRE(loaded.known[0].message == kSF7.message);
  REQUIRE(loaded.known[1].ruleId == kSF9.ruleId);
  REQUIRE(loaded.known[1].file == kSF9.file);
}

TEST_CASE("baseline: empty violations round-trip", "[baseline]")
{
  TempFile tmp("archcheck-baseline-empty.json");
  saveBaseline(ViolationBaseline{}, tmp.path);
  const auto loaded = loadBaseline(tmp.path);
  REQUIRE(loaded.known.empty());
}

TEST_CASE("baseline: special chars in message survive round-trip", "[baseline]")
{
  TempFile tmp("archcheck-baseline-special.json");
  ViolationBaseline b;
  b.known.push_back({"SF.9", "a.h", 0, "cycle: a → b → \"c\" \\ a"});
  saveBaseline(b, tmp.path);
  const auto loaded = loadBaseline(tmp.path);
  REQUIRE(loaded.known[0].message == "cycle: a → b → \"c\" \\ a");
}

TEST_CASE("filterNew: known violation is suppressed", "[baseline]")
{
  const ViolationBaseline b{{kSF7}};
  const auto result = filterNew({kSF7}, b);
  REQUIRE(result.empty());
}

TEST_CASE("filterNew: different line is treated as new", "[baseline]")
{
  const ViolationBaseline b{{kSF7}};
  const Violation diff_line{"SF.7", "include/bar.h", 13, "using namespace std"};
  const auto result = filterNew({diff_line}, b);
  REQUIRE(result.size() == 1);
}

TEST_CASE("filterNew: different rule is treated as new", "[baseline]")
{
  const ViolationBaseline b{{kSF7}};
  const Violation diff_rule{"SF.8", "include/bar.h", 12, "missing include guard"};
  const auto result = filterNew({diff_rule}, b);
  REQUIRE(result.size() == 1);
}

TEST_CASE("filterNew: unknown violation passes through, known is suppressed", "[baseline]")
{
  const ViolationBaseline b{{kSF7}};
  const auto result = filterNew({kSF7, kSF9}, b);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0].ruleId == kSF9.ruleId);
}

TEST_CASE("loadBaseline: missing file throws BaselineError", "[baseline]")
{
  REQUIRE_THROWS_AS(loadBaseline("/tmp/archcheck-nonexistent-xyz.json"), BaselineError);
}

TEST_CASE("loadBaseline: invalid content throws BaselineError", "[baseline]")
{
  TempFile tmp("archcheck-baseline-garbage.json");
  {
    std::ofstream f(tmp.path);
    f << "this is not json at all\n";
  }
  REQUIRE_THROWS_AS(loadBaseline(tmp.path), BaselineError);
}
