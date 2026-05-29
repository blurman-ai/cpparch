#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "archcheck/scan/include_scanner.h"

using archcheck::scan::DiagnosticKind;
using archcheck::scan::IncludeDirective;
using archcheck::scan::IncludeKind;
using archcheck::scan::scanIncludes;

namespace
{

bool equal(const IncludeDirective &d, IncludeKind k, std::string_view token, int line)
{
  return d.kind == k && d.token == token && d.line == line;
}

std::vector<IncludeDirective> extract_directives(std::string_view source) { return scanIncludes(source).directives; }

} // namespace

TEST_CASE("scan_includes on empty source returns empty", "[scan][scanner]") { REQUIRE(extract_directives("").empty()); }

TEST_CASE("scan_includes extracts a single quote include", "[scan][scanner]")
{
  const auto res = extract_directives("#include \"foo.h\"\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "foo.h", 1));
}

TEST_CASE("scan_includes extracts a single angle include", "[scan][scanner]")
{
  const auto res = extract_directives("#include <vector>\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Angle, "vector", 1));
}

TEST_CASE("scan_includes returns multiple directives with line numbers", "[scan][scanner]")
{
  const auto res = extract_directives("#include <a>\n#include \"b\"\n#include <c>\n");
  REQUIRE(res.size() == 3);
  REQUIRE(equal(res[0], IncludeKind::Angle, "a", 1));
  REQUIRE(equal(res[1], IncludeKind::Quote, "b", 2));
  REQUIRE(equal(res[2], IncludeKind::Angle, "c", 3));
}

TEST_CASE("scan_includes ignores files without includes", "[scan][scanner]")
{
  REQUIRE(extract_directives("int main() { return 0; }\n").empty());
}

TEST_CASE("scan_includes accepts leading spaces and tabs", "[scan][scanner]")
{
  const auto res = extract_directives("   #include <x>\n\t#include \"y\"\n");
  REQUIRE(res.size() == 2);
  REQUIRE(equal(res[0], IncludeKind::Angle, "x", 1));
  REQUIRE(equal(res[1], IncludeKind::Quote, "y", 2));
}

TEST_CASE("scan_includes handles last line without trailing newline", "[scan][scanner]")
{
  const auto res = extract_directives("#include <x>");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Angle, "x", 1));
}

TEST_CASE("scan_includes ignores #include inside // line comment", "[scan][scanner][comments]")
{
  REQUIRE(extract_directives("// #include \"x\"\n").empty());
}

TEST_CASE("scan_includes keeps directive when // appears after the token", "[scan][scanner][comments]")
{
  const auto res = extract_directives("#include \"x\" // trailing comment\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "x", 1));
}

TEST_CASE("scan_includes ignores #include inside single-line /* */ block", "[scan][scanner][comments]")
{
  REQUIRE(extract_directives("/* #include \"x\" */\n").empty());
}

TEST_CASE("scan_includes ignores #include inside multi-line block comment", "[scan][scanner][comments]")
{
  const auto res = extract_directives("/*\n#include \"x\"\n*/\n#include <y>\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Angle, "y", 4));
}

TEST_CASE("scan_includes treats unterminated /* as swallowing the rest of the file", "[scan][scanner][comments]")
{
  REQUIRE(extract_directives("/* unterminated\n#include \"x\"\n").empty());
}

TEST_CASE("scan_includes does not false-match #include inside an ordinary string literal", "[scan][scanner][strings]")
{
  REQUIRE(extract_directives("const char* s = \"#include \\\"x\\\"\";\n").empty());
}

TEST_CASE("scan_includes does not false-match # inside a character literal", "[scan][scanner][strings]")
{
  REQUIRE(extract_directives("char c = '#';\n").empty());
}

TEST_CASE("scan_includes ignores #include inside single-line raw string", "[scan][scanner][strings]")
{
  REQUIRE(extract_directives("auto s = R\"(#include \"x\")\";\n").empty());
}

TEST_CASE("scan_includes ignores #include inside raw string with custom delimiter", "[scan][scanner][strings]")
{
  REQUIRE(extract_directives("auto s = R\"d(#include \"x\")d\";\n").empty());
}

TEST_CASE("scan_includes ignores #include inside multi-line raw string", "[scan][scanner][strings]")
{
  const auto res = extract_directives("auto s = R\"(\n#include \"x\"\n)\";\n#include <y>\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Angle, "y", 4));
}

TEST_CASE("scan_includes splices continuation inside #include keyword", "[scan][scanner][continuation]")
{
  const auto res = extract_directives("#inc\\\nlude \"x\"\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "x", 1));
}

TEST_CASE("scan_includes splices continuation between #include and token", "[scan][scanner][continuation]")
{
  const auto res = extract_directives("#include \\\n\"x\"\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "x", 1));
}

TEST_CASE("scan_includes splices continuation inside include token", "[scan][scanner][continuation]")
{
  const auto res = extract_directives("#include \"fo\\\no.h\"\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "foo.h", 1));
}

TEST_CASE("scan_includes splices multiple consecutive continuations", "[scan][scanner][continuation]")
{
  const auto res = extract_directives("#in\\\nclu\\\nde <x>\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Angle, "x", 1));
}

TEST_CASE("scan_includes does not splice backslash without immediate newline", "[scan][scanner][continuation]")
{
  REQUIRE(extract_directives("#include \\ \n<x>\n").empty());
}

TEST_CASE("scan_includes preserves physical line numbers after splice", "[scan][scanner][continuation]")
{
  const auto res = extract_directives("// comment\n#in\\\nclude <a>\n#include <b>\n");
  REQUIRE(res.size() == 2);
  REQUIRE(equal(res[0], IncludeKind::Angle, "a", 2));
  REQUIRE(equal(res[1], IncludeKind::Angle, "b", 4));
}

TEST_CASE("scan_includes rejects #include preceded by code on the same line", "[scan][scanner][first-sig]")
{
  REQUIRE(extract_directives("int x; #include \"y\"\n").empty());
}

TEST_CASE("scan_includes rejects #include preceded by a single non-ws char", "[scan][scanner][first-sig]")
{
  REQUIRE(extract_directives("; #include \"y\"\n").empty());
}

TEST_CASE("scan_includes finds only the first #include on a line", "[scan][scanner][first-sig]")
{
  const auto res = extract_directives("#include \"a\" #include \"b\"\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "a", 1));
}

TEST_CASE("scan_includes rejects splice-joined line where # is no longer first-significant",
          "[scan][scanner][first-sig]")
{
  REQUIRE(extract_directives("int x; \\\n#include \"y\"\n").empty());
}

TEST_CASE("scan_includes emits MacroInclude diagnostic for #include FOO", "[scan][scanner][macro]")
{
  const auto res = scanIncludes("#include FOO\n");
  REQUIRE(res.directives.empty());
  REQUIRE(res.diagnostics.size() == 1);
  REQUIRE(res.diagnostics[0].kind == DiagnosticKind::MacroInclude);
  REQUIRE(res.diagnostics[0].rawToken == "FOO");
  REQUIRE(res.diagnostics[0].line == 1);
}

TEST_CASE("scan_includes captures the full identifier for macro include", "[scan][scanner][macro]")
{
  const auto res = scanIncludes("#include  BAR_X1\n");
  REQUIRE(res.directives.empty());
  REQUIRE(res.diagnostics.size() == 1);
  REQUIRE(res.diagnostics[0].rawToken == "BAR_X1");
}

TEST_CASE("scan_includes does not emit a diagnostic for a real quote include", "[scan][scanner][macro]")
{
  const auto res = scanIncludes("#include \"x\"\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.diagnostics.empty());
}

TEST_CASE("scan_includes ignores an empty #include with no token", "[scan][scanner][macro]")
{
  const auto res = scanIncludes("#include\n");
  REQUIRE(res.directives.empty());
  REQUIRE(res.diagnostics.empty());
}

TEST_CASE("scan_includes marks include inside #ifdef as conditional", "[scan][scanner][conditional]")
{
  const auto res = scanIncludes("#ifdef MACRO\n#include \"foo.h\"\n#endif\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].token == "foo.h");
  REQUIRE(res.directives[0].conditional == true);
}

TEST_CASE("scan_includes marks include outside #ifdef as non-conditional", "[scan][scanner][conditional]")
{
  const auto res = scanIncludes("#include \"bar.h\"\n#ifdef MACRO\n#endif\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].token == "bar.h");
  REQUIRE(res.directives[0].conditional == false);
}

TEST_CASE("scan_includes handles nested #if blocks correctly", "[scan][scanner][conditional]")
{
  const auto res = scanIncludes(
      "#if A\n#include \"a.h\"\n#if B\n#include \"b.h\"\n#endif\n#include \"c.h\"\n#endif\n#include \"d.h\"\n");
  REQUIRE(res.directives.size() == 4);
  REQUIRE(res.directives[0].token == "a.h");
  REQUIRE(res.directives[0].conditional == true);
  REQUIRE(res.directives[1].token == "b.h");
  REQUIRE(res.directives[1].conditional == true);
  REQUIRE(res.directives[2].token == "c.h");
  REQUIRE(res.directives[2].conditional == true);
  REQUIRE(res.directives[3].token == "d.h");
  REQUIRE(res.directives[3].conditional == false);
}

TEST_CASE("scan_includes strips UTF-8 BOM before first #include", "[scan][scanner][bom]")
{
  const auto res = extract_directives("\xEF\xBB\xBF#include \"foo.h\"\n");
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "foo.h", 1));
}

TEST_CASE("scan_includes with BOM yields the same set as without BOM", "[scan][scanner][bom]")
{
  const std::string_view body = "#pragma once\n#include \"a.h\"\n#include <b.h>\n";
  const auto with_bom = scanIncludes(std::string("\xEF\xBB\xBF") + std::string(body));
  const auto without_bom = scanIncludes(body);
  REQUIRE(with_bom.directives.size() == without_bom.directives.size());
  for (std::size_t i = 0; i < without_bom.directives.size(); ++i)
  {
    REQUIRE(with_bom.directives[i].kind == without_bom.directives[i].kind);
    REQUIRE(with_bom.directives[i].token == without_bom.directives[i].token);
    REQUIRE(with_bom.directives[i].line == without_bom.directives[i].line);
  }
}

TEST_CASE("scan_includes: top-level include in #ifndef-guarded file is not conditional",
          "[scan][scanner][conditional][guard]")
{
  const auto res = scanIncludes("#ifndef MY_HEADER_H\n#define MY_HEADER_H\n#include \"foo.h\"\n#endif\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].token == "foo.h");
  REQUIRE(res.directives[0].conditional == false);
}

TEST_CASE("scan_includes: guard recognized after leading blank lines and comments",
          "[scan][scanner][conditional][guard]")
{
  const auto res = scanIncludes("// SPDX-License-Identifier: Apache-2.0\n\n/* copyright */\n\n"
                                "#ifndef GRPC_FOO_H\n#define GRPC_FOO_H\n#include \"bar.h\"\n#endif\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].conditional == false);
}

TEST_CASE("scan_includes: nested #ifdef inside include-guard still marks include conditional",
          "[scan][scanner][conditional][guard]")
{
  const auto res =
      scanIncludes("#ifndef MY_H\n#define MY_H\n#include \"top.h\"\n#ifdef OPT\n#include \"opt.h\"\n#endif\n#endif\n");
  REQUIRE(res.directives.size() == 2);
  REQUIRE(res.directives[0].token == "top.h");
  REQUIRE(res.directives[0].conditional == false);
  REQUIRE(res.directives[1].token == "opt.h");
  REQUIRE(res.directives[1].conditional == true);
}

TEST_CASE("scan_includes: #ifndef without matching #define is not a guard", "[scan][scanner][conditional][guard]")
{
  const auto res = scanIncludes("#ifndef MY_H\n#include \"foo.h\"\n#endif\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].conditional == true);
}

TEST_CASE("scan_includes: #ifndef/#define with mismatched identifier is not a guard",
          "[scan][scanner][conditional][guard]")
{
  const auto res = scanIncludes("#ifndef MY_H\n#define OTHER_H\n#include \"foo.h\"\n#endif\n");
  REQUIRE(res.directives.size() == 1);
  REQUIRE(res.directives[0].conditional == true);
}

TEST_CASE("scan_includes does not strip BOM that appears later in the source", "[scan][scanner][bom]")
{
  // Only a leading BOM is meaningful; an embedded BOM stays in source.
  const auto res = extract_directives("#include \"foo.h\"\n\xEF\xBB\xBF#include \"bar.h\"\n");
  // The second #include is preceded by BOM, so it is not first-significant on its line.
  REQUIRE(res.size() == 1);
  REQUIRE(equal(res[0], IncludeKind::Quote, "foo.h", 1));
}
