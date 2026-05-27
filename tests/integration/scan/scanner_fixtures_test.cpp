#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "archcheck/scan/include_scanner.h"

using archcheck::scan::DiagnosticKind;
using archcheck::scan::IncludeKind;
using archcheck::scan::scanIncludes;

namespace
{

std::filesystem::path fixture(std::string_view name)
{
  return std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / "scan" / "include_scanner" / name;
}

std::string read(const std::filesystem::path &p)
{
  std::ifstream f(p, std::ios::binary);
  REQUIRE(f.is_open());
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("fixture: simple — angle + quote + angle, по одному на строку", "[scan][fixtures]")
{
  const auto res = scanIncludes(read(fixture("simple.cpp")));
  REQUIRE(res.diagnostics.empty());
  REQUIRE(res.directives.size() == 3);
  REQUIRE(res.directives[0].kind == IncludeKind::Angle);
  REQUIRE(res.directives[0].token == "vector");
  REQUIRE(res.directives[1].kind == IncludeKind::Quote);
  REQUIRE(res.directives[1].token == "local.h");
  REQUIRE(res.directives[2].kind == IncludeKind::Angle);
  REQUIRE(res.directives[2].token == "string");
}

TEST_CASE("fixture: comments — //, /* */ multi-line", "[scan][fixtures]")
{
  const auto res = scanIncludes(read(fixture("comments.cpp")));
  REQUIRE(res.diagnostics.empty());
  REQUIRE(res.directives.size() == 2);
  REQUIRE(res.directives[0].token == "real.h");
  REQUIRE(res.directives[0].line == 2);
  REQUIRE(res.directives[1].token == "fine");
  REQUIRE(res.directives[1].line == 6);
}

TEST_CASE("fixture: string_literal — #include внутри строки не ловится", "[scan][fixtures]")
{
  const auto res = scanIncludes(read(fixture("string_literal.cpp")));
  REQUIRE(res.diagnostics.empty());
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].token == "real.h");
  REQUIRE(res.directives[0].line == 2);
}

TEST_CASE("fixture: raw_string — содержимое R\"(...)\" игнорируется", "[scan][fixtures]")
{
  const auto res = scanIncludes(read(fixture("raw_string.cpp")));
  REQUIRE(res.diagnostics.empty());
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].token == "after");
  REQUIRE(res.directives[0].line == 4);
}

TEST_CASE("fixture: continuation — \\-EOL внутри #include склеивается", "[scan][fixtures]")
{
  const auto res = scanIncludes(read(fixture("continuation.cpp")));
  REQUIRE(res.diagnostics.empty());
  REQUIRE(res.directives.size() == 2);
  REQUIRE(res.directives[0].token == "split.h");
  REQUIRE(res.directives[0].line == 1);
  REQUIRE(res.directives[1].token == "normal");
  REQUIRE(res.directives[1].line == 3);
}

TEST_CASE("fixture: macro_include — #include CONFIG_HEADER → diagnostic", "[scan][fixtures]")
{
  const auto res = scanIncludes(read(fixture("macro_include.cpp")));
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].token == "real.h");
  REQUIRE(res.diagnostics.size() == 1);
  REQUIRE(res.diagnostics[0].kind == DiagnosticKind::MacroInclude);
  REQUIRE(res.diagnostics[0].rawToken == "CONFIG_HEADER");
  REQUIRE(res.diagnostics[0].line == 1);
}
