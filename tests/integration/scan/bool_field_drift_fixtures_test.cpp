#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "archcheck/scan/bool_field_drift.h"

namespace
{

std::string fixture(const std::string &name)
{
  const auto path = std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / "bool_field_drift" / name;
  std::ifstream f(path);
  REQUIRE(f.is_open());
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

TEST_CASE("bool_drift fixtures: pre-existing struct gaining bools is a finding", "[scan][bool_drift][fixtures]")
{
  const auto res = archcheck::scan::compareBoolFields(fixture("fail_accretion/options_baseline.h"),
                                                      fixture("fail_accretion/options_current.h"), "options.h");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].ruleId == "DRIFT.BOOL_FIELD_ACCRETION");
  REQUIRE(res.violations[0].message.find("accreted 2") != std::string::npos);
  REQUIRE(res.violations[0].message.find("fullscreen, hdr") != std::string::npos);
}

TEST_CASE("bool_drift fixtures: a rename keeps the count and is silent", "[scan][bool_drift][fixtures]")
{
  const auto res = archcheck::scan::compareBoolFields(fixture("pass/rename_baseline.h"),
                                                      fixture("pass/rename_current.h"), "session.h");
  REQUIRE(res.violations.empty());
}

TEST_CASE("bool_drift fixtures: brace in a string literal does not fake accretion", "[scan][bool_drift][fixtures]")
{
  const auto res = archcheck::scan::compareBoolFields(fixture("pass/literal_brace_baseline.h"),
                                                      fixture("pass/literal_brace_current.h"), "writer.h");
  REQUIRE(res.violations.empty());
}

} // namespace
