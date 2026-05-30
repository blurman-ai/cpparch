#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/report/json_escape.h"

using archcheck::report::jsonEscape;

TEST_CASE("jsonEscape leaves plain text untouched", "[report][json_escape]")
{
  REQUIRE(jsonEscape("") == "");
  REQUIRE(jsonEscape("plain/path.cpp") == "plain/path.cpp");
}

TEST_CASE("jsonEscape escapes the JSON metacharacters", "[report][json_escape]")
{
  REQUIRE(jsonEscape("a\"b") == "a\\\"b"); // double quote
  REQUIRE(jsonEscape("a\\b") == "a\\\\b"); // backslash
  REQUIRE(jsonEscape("a\nb") == "a\\nb");  // newline
}

TEST_CASE("jsonEscape handles all three together", "[report][json_escape]")
{
  REQUIRE(jsonEscape("\"\\\n") == "\\\"\\\\\\n");
}
