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

TEST_CASE("jsonEscape escapes all 32 control characters (S5)", "[report][json_escape][security]")
{
  // Build a string containing all 32 control characters U+0000..U+001F.
  std::string ctrl;
  for (int i = 0; i < 32; ++i)
  {
    ctrl += static_cast<char>(i);
  }
  const std::string escaped = jsonEscape(ctrl);

  // Verify known short-form escapes are present.
  REQUIRE(escaped.find("\\b") != std::string::npos); // 0x08
  REQUIRE(escaped.find("\\t") != std::string::npos); // 0x09
  REQUIRE(escaped.find("\\n") != std::string::npos); // 0x0A
  REQUIRE(escaped.find("\\f") != std::string::npos); // 0x0C
  REQUIRE(escaped.find("\\r") != std::string::npos); // 0x0D

  // No raw control characters must survive (all must be escaped).
  for (const char c : escaped)
  {
    REQUIRE(static_cast<unsigned char>(c) >= 0x20);
  }

  // The result must be embeddable as a JSON string value (wrap and check structure).
  const std::string json = "\"" + escaped + "\"";
  // Simple structural check: starts with '"', ends with '"', no unescaped control chars.
  REQUIRE(json.front() == '"');
  REQUIRE(json.back() == '"');
}

TEST_CASE("jsonEscape handles short-form escapes individually", "[report][json_escape]")
{
  REQUIRE(jsonEscape("\r") == "\\r");
  REQUIRE(jsonEscape("\t") == "\\t");
  REQUIRE(jsonEscape("\b") == "\\b");
  REQUIRE(jsonEscape("\f") == "\\f");
}

TEST_CASE("jsonEscape emits \\uXXXX for other control characters", "[report][json_escape]")
{
  // U+0001 (SOH) -> , U+001F (US) -> 
  REQUIRE(jsonEscape("\x01") == "\\u0001");
  REQUIRE(jsonEscape("\x1f") == "\\u001f");
  // U+0000: build via std::string to avoid null-terminator truncation.
  const std::string nul(1, '\x00');
  REQUIRE(jsonEscape(nul) == "\\u0000");
}

TEST_CASE("jsonEscape preserves valid UTF-8", "[report][json_escape]")
{
  // UTF-8 two-byte sequence: U+00E9 (é) = 0xC3 0xA9
  const std::string utf8_e_acute = "\xC3\xA9";
  REQUIRE(jsonEscape(utf8_e_acute) == utf8_e_acute);
  // Three-byte: U+20AC (€) = 0xE2 0x82 0xAC
  const std::string utf8_euro = "\xE2\x82\xAC";
  REQUIRE(jsonEscape(utf8_euro) == utf8_euro);
}

TEST_CASE("jsonEscape replaces invalid UTF-8 with U+FFFD (S5)", "[report][json_escape][security]")
{
  // Lone continuation byte (0x80 alone is not valid UTF-8).
  const std::string bad = "\x80";
  const std::string result = jsonEscape(bad);
  // Must be replaced by UTF-8 encoding of U+FFFD: EF BF BD
  REQUIRE(result == "\xEF\xBF\xBD");
}
