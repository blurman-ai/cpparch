#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/scan/duplication/token_normalizer.h"
#include "archcheck/scan/flag_argument_scan.h"

using namespace archcheck::scan;

namespace
{
archcheck::rules::ViolationList scan(const std::string &src)
{
  return detectFlagArguments(duplication::lex(src), "t.cpp");
}
} // namespace

TEST_CASE("flag-arg: two boolean parameters fire ARG.1", "[scan][flagarg]")
{
  const auto v = scan("void f(bool a, bool b) { use(); }");
  REQUIRE(v.size() == 1);
  REQUIRE(v[0].ruleId == "ARG.1.flag_argument_signature");
}

TEST_CASE("flag-arg: single boolean with a flag-like name fires", "[scan][flagarg]")
{
  REQUIRE(scan("void f(int x, bool enableCache) { use(); }").size() == 1);
  REQUIRE(scan("void g(bool skip_validation) { use(); }").size() == 1);
}

TEST_CASE("flag-arg: single boolean with a plain name stays silent", "[scan][flagarg]")
{
  // 'node'/'normal' start with the 'no' prefix but not on a word boundary.
  REQUIRE(scan("void f(bool node) { use(); }").empty());
  REQUIRE(scan("void f(bool ready) { use(); }").empty());
}

TEST_CASE("flag-arg: bool* / bool& are out-parameters, not flags", "[scan][flagarg]")
{
  REQUIRE(scan("void f(bool* ok, bool& done) { use(); }").empty());
}

TEST_CASE("flag-arg: no boolean parameters stays silent", "[scan][flagarg]")
{
  REQUIRE(scan("int f(int a, const char* s) { return a; }").empty());
}

TEST_CASE("flag-arg: bool inside a template argument is not a parameter", "[scan][flagarg]")
{
  REQUIRE(scan("void f(std::vector<bool> bits) { use(); }").empty());
}
