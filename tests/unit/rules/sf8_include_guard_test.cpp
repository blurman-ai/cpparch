#include <catch2/catch_test_macros.hpp>

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
