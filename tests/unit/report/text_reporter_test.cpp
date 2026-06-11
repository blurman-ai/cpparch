#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "archcheck/report/text_reporter.h"
#include "archcheck/rules/violation.h"

using archcheck::rules::Violation;
using archcheck::rules::ViolationList;

TEST_CASE("writeTextReport: 1 violation, useColor=false (no ANSI codes)", "[report][text_reporter]")
{
  ViolationList violations;
  violations.push_back(
      Violation{.ruleId = "SF.9", .file = "src/main.cpp", .line = 42, .message = "cyclic include detected"});

  std::ostringstream oss;
  archcheck::report::writeTextReport(violations, oss, false);
  const std::string output = oss.str();

  // Count occurrences of escape code marker
  const int escapeCount = std::count(output.begin(), output.end(), '\033');
  REQUIRE(escapeCount == 0);
}

TEST_CASE("writeTextReport: 1 violation, useColor=true (includes ANSI red)", "[report][text_reporter]")
{
  ViolationList violations;
  violations.push_back(
      Violation{.ruleId = "SF.9", .file = "src/main.cpp", .line = 42, .message = "cyclic include detected"});

  std::ostringstream oss;
  archcheck::report::writeTextReport(violations, oss, true);
  const std::string output = oss.str();

  // Must contain red color code around ruleId and summary
  REQUIRE(output.find("\033[31m[") != std::string::npos); // Red code before [
  REQUIRE(output.find("\033[0m") != std::string::npos);   // Reset code present
}

TEST_CASE("writeTextReport: empty list, useColor=true (green 'No violations')", "[report][text_reporter]")
{
  ViolationList violations;

  std::ostringstream oss;
  archcheck::report::writeTextReport(violations, oss, true);
  const std::string output = oss.str();

  // Must contain green "No violations found"
  REQUIRE(output.find("\033[32mNo violations found.\033[0m") != std::string::npos);
}

TEST_CASE("writeTextReport: backward compatibility with 2-arg call", "[report][text_reporter]")
{
  ViolationList violations;
  violations.push_back(
      Violation{.ruleId = "SF.9", .file = "src/main.cpp", .line = 42, .message = "cyclic include detected"});

  std::ostringstream oss;
  // Call without third argument (defaults to useColor=false)
  archcheck::report::writeTextReport(violations, oss);
  const std::string output = oss.str();

  // Should be identical to explicitly passing false
  std::ostringstream ossExplicit;
  archcheck::report::writeTextReport(violations, ossExplicit, false);
  const std::string outputExplicit = ossExplicit.str();

  REQUIRE(output == outputExplicit);
}
