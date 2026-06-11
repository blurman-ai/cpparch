#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "archcheck/git/diff_query.h"
#include "archcheck/scan/test_co_evolution.h"

namespace
{

using NumStatVec = std::vector<archcheck::git::NumStat>;

NumStatVec parseNumstatFixture(const std::string &fixture)
{
  NumStatVec result;
  std::istringstream iss{fixture};
  std::string line;
  while (std::getline(iss, line))
  {
    if (line.empty())
      continue;
    const auto tab1 = line.find('\t');
    if (tab1 == std::string::npos)
      continue;
    const auto tab2 = line.find('\t', tab1 + 1);
    if (tab2 == std::string::npos)
      continue;
    try
    {
      const int added = std::stoi(line.substr(0, tab1));
      const int removed = std::stoi(line.substr(tab1 + 1, tab2 - tab1 - 1));
      const auto path = line.substr(tab2 + 1);
      result.push_back({path, added, removed});
    }
    catch (...)
    {
      // skip unparseable
    }
  }
  return result;
}

} // namespace

TEST_CASE("test_co_evolution: prod 100 lines + tests 0 → finding", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("50\t30\tsrc/main.cpp\n"
                                           "20\t0\tsrc/handler.h\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "TEST.1.prod_changed_tests_silent");
  REQUIRE(violations[0].file == "<diff>");
  REQUIRE(violations[0].line == 0);
  // Total: 50+20=70 added, 30+0=30 removed, 2 files
  REQUIRE(violations[0].message.find("+70/-30") != std::string::npos);
  REQUIRE(violations[0].message.find("2 file(s)") != std::string::npos);
  REQUIRE(violations[0].message.find("+0/-0") != std::string::npos);
}

TEST_CASE("test_co_evolution: prod 100 + tests 20 → no finding", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("50\t30\tsrc/main.cpp\n"
                                           "20\t10\ttests/test_main.cpp\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.empty());
}

TEST_CASE("test_co_evolution: docs-only → no finding", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("100\t50\tREADME.md\n"
                                           "20\t10\tdocs/api.md\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.empty());
}

TEST_CASE("test_co_evolution: rename-only → ignored for churn", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("0\t0\tsrc/old.cpp => src/new.cpp\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.empty());
}

TEST_CASE("test_co_evolution: prod 250 + tests 5 (ratio 2%) → finding (soft threshold)", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("150\t100\tsrc/main.cpp\n"
                                           "4\t1\ttests/test_main.cpp\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "TEST.1.prod_changed_tests_silent");
}

TEST_CASE("test_co_evolution: vendor files not counted as prod", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("100\t50\tsrc/vendor/sqlite3.cpp\n"
                                           "20\t10\tthird_party/json/json.cpp\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.empty());
}

TEST_CASE("test_co_evolution: mixed vendor + prod + tests", "[scan][test_co_evolution]")
{
  const auto numstat = parseNumstatFixture("100\t50\tsrc/vendor/sqlite3.cpp\n"
                                           "40\t20\tsrc/main.cpp\n"
                                           "5\t2\ttests/test_main.cpp\n");
  // Prod churn = 40+20 = 60 (below 80 threshold, no finding)
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.empty());
}

TEST_CASE("test_co_evolution: soft threshold boundary", "[scan][test_co_evolution]")
{
  // prod_churn = 200 exactly, test_churn / prod_churn = 10/200 = 0.05 (exactly 5%)
  // Should NOT trigger (< not <=)
  const auto numstat = parseNumstatFixture("120\t80\tsrc/main.cpp\n"
                                           "8\t2\ttests/test_main.cpp\n");
  const auto violations = archcheck::scan::detectTestCoEvolution(numstat);
  REQUIRE(violations.empty());
}
