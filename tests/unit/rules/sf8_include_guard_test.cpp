#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/sf8_include_guard.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::Sf8IncludeGuard;

static auto makeReadFile(std::string content)
{
  return [c = std::move(content)](std::string_view) { return c; };
}

TEST_CASE("SF.8: pragma once → no violation", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("#pragma once\nclass Foo {};\n")).empty());
}

TEST_CASE("SF.8: ifndef guard → no violation", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("#ifndef A_H\n#define A_H\nclass Foo {};\n#endif\n")).empty());
}

TEST_CASE("SF.8: lone #ifndef without matching #define is not a guard", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  const auto v = rule.check(g, makeReadFile("#ifndef NDEBUG\n#include <cassert>\n#endif\nclass Foo {};\n"));
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "SF.8");
}

TEST_CASE("SF.8: #define of a different macro does not close the guard", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("#ifndef A_H\n#define OTHER_MACRO 1\n#endif\n")).size() == 1);
}

TEST_CASE("SF.8: guard pair with comment line between → no violation", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("#ifndef A_H\n// guard\n#define A_H\n#endif\n")).empty());
}

TEST_CASE("SF.8: missing guard → violation on line 1", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  const auto v = rule.check(g, makeReadFile("class Foo {};\n"));
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "SF.8");
  CHECK(v[0].line == 1);
  CHECK(v[0].file == "a.h");
}

TEST_CASE("SF.8: .cpp source is not checked", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("main.cpp");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("class Foo {};\n")).empty());
}

TEST_CASE("SF.8: empty file reports violation", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("empty.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("")).size() == 1);
}

TEST_CASE("SF.8: long copyright block + ifndef guard → no violation", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("a.h");

  // 32 non-empty comment lines before the guard — models Apache 2.0 copyright
  // + module description (abseil-style). Exceeds old kScanLines=30 limit.
  std::string src;
  for (int i = 0; i < 32; ++i)
    src += "// copyright line\n";
  src += "#ifndef LONG_GUARD_H_\n#define LONG_GUARD_H_\n#endif\n";

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile(src)).empty());
}

TEST_CASE("SF.8: .inc fragment is not checked", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("platform.inc");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("// platform fragment, no guard\n")).empty());
}

TEST_CASE("SF.8: Objective-C header with @interface is not checked", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("AppDelegate.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("#import <UIKit/UIKit.h>\n"
                                     "@interface AppDelegate : UIResponder\n"
                                     "@end\n"))
              .empty());
}

TEST_CASE("SF.8: Objective-C header with #import is not checked", "[rules][sf8]")
{
  DependencyGraph g;
  g.addNode("Foo.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("// Apache 2.0 boilerplate\n"
                                     "#import <Foundation/Foundation.h>\n"
                                     "@implementation Foo\n"
                                     "@end\n"))
              .empty());
}

TEST_CASE("SF.8: UTF-8 BOM before #pragma once → no violation", "[rules][sf8][bom]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("\xEF\xBB\xBF#pragma once\nclass Foo {};\n")).empty());
}

TEST_CASE("SF.8: UTF-8 BOM before #ifndef guard → no violation", "[rules][sf8][bom]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf8IncludeGuard rule;
  REQUIRE(rule.check(g, makeReadFile("\xEF\xBB\xBF#ifndef A_H\n#define A_H\nclass Foo {};\n#endif\n")).empty());
}
