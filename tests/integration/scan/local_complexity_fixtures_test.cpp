#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "archcheck/scan/local_complexity_drift.h"
#include "archcheck/scan/local_complexity_metrics.h"

namespace
{

std::string fixture(const std::string &name)
{
  const auto path = std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / "local_complexity_drift" / name;
  std::ifstream f(path);
  REQUIRE(f.is_open());
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

TEST_CASE("complexity fixtures: flat branches stay below any alarm", "[scan][complexity][fixtures]")
{
  const auto fns = archcheck::scan::computeFileComplexity(fixture("pass/flat_branches.cpp"));
  REQUIRE(fns.size() == 1);
  REQUIRE(fns[0].score == 3);
  REQUIRE(fns[0].maxNesting <= 1);
}

TEST_CASE("complexity fixtures: comments and strings carry no branch tokens", "[scan][complexity][fixtures]")
{
  const auto fns = archcheck::scan::computeFileComplexity(fixture("pass/comments_and_strings.cpp"));
  REQUIRE(fns.size() == 1);
  REQUIRE(fns[0].score == 0);
}

TEST_CASE("complexity fixtures: preprocessor branches score 0", "[scan][complexity][fixtures]")
{
  const auto fns = archcheck::scan::computeFileComplexity(fixture("pass/preprocessor_lines.cpp"));
  REQUIRE(fns.size() == 1);
  REQUIRE(fns[0].score == 0);
}

TEST_CASE("complexity fixtures: nested growth baseline -> current is a finding", "[scan][complexity][fixtures]")
{
  const auto res = archcheck::scan::compareLocalComplexity(fixture("fail_growth/update_baseline.cpp"),
                                                           fixture("fail_growth/update_current.cpp"), "update.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].ruleId == "DRIFT.LOCAL_COMPLEXITY");
  REQUIRE(res.violations[0].message.find("from 1 to 10") != std::string::npos);
}

TEST_CASE("complexity fixtures: harmless append is silent", "[scan][complexity][fixtures]")
{
  const auto res = archcheck::scan::compareLocalComplexity(fixture("pass/harmless_change_baseline.cpp"),
                                                           fixture("pass/harmless_change_current.cpp"), "act.cpp");
  REQUIRE(res.violations.empty());
  REQUIRE(res.positiveDelta == 0);
}

TEST_CASE("complexity fixtures: new function above 25 is reported", "[scan][complexity][fixtures]")
{
  const auto res =
      archcheck::scan::compareLocalComplexity("", fixture("fail_new_complex/new_complex_function.cpp"), "new.cpp");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("new function 'saturate'") != std::string::npos);
}

} // namespace
