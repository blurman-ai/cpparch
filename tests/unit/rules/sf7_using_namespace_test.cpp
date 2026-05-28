#include <catch2/catch_test_macros.hpp>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/sf7_using_namespace.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::Sf7UsingNamespace;

static auto makeReadFile(std::string content)
{
  return [c = std::move(content)](std::string_view) { return c; };
}

TEST_CASE("SF.7: clean header has no violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  REQUIRE(rule.check(g, makeReadFile("#pragma once\nclass Foo {};\n")).empty());
}

TEST_CASE("SF.7: using namespace std in header → violation on correct line", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto v = rule.check(g, makeReadFile("#pragma once\nusing namespace std;\n"));
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "SF.7");
  CHECK(v[0].line == 2);
  CHECK(v[0].file == "a.h");
}

TEST_CASE("SF.7: using namespace in .cpp source — no violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("main.cpp");

  Sf7UsingNamespace rule;
  REQUIRE(rule.check(g, makeReadFile("using namespace std;\n")).empty());
}

TEST_CASE("SF.7: using namespace in line comment — no violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  REQUIRE(rule.check(g, makeReadFile("// using namespace std;\n")).empty());
}

TEST_CASE("SF.7: multiple violations in one file", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto src = "#pragma once\nusing namespace std;\nusing namespace boost;\n";
  const auto v = rule.check(g, makeReadFile(src));
  REQUIRE(v.size() == 2);
  CHECK(v[0].line == 2);
  CHECK(v[1].line == 3);
}

TEST_CASE("SF.7: using namespace inside function body — no violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto src = "#pragma once\n"
                   "struct S {\n"
                   "  static std::string f() {\n"
                   "    using namespace std::string_literals;\n"
                   "    return \"x\"s;\n"
                   "  }\n"
                   "};\n";
  REQUIRE(rule.check(g, makeReadFile(src)).empty());
}

TEST_CASE("SF.7: using namespace inside inline lambda — no violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto src = "#pragma once\n"
                   "auto g = []{ using namespace Catch::Generators; return 42; };\n";
  REQUIRE(rule.check(g, makeReadFile(src)).empty());
}

TEST_CASE("SF.7: using namespace after closing brace — violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto src = "#pragma once\n"
                   "struct Foo {};\n"
                   "using namespace std;\n";
  const auto v = rule.check(g, makeReadFile(src));
  REQUIRE(v.size() == 1);
  CHECK(v[0].line == 3);
}

TEST_CASE("SF.7: using namespace inside block comment — no violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto src = "#pragma once\n"
                   "/**\n"
                   " * \\code\n"
                   " *   using namespace folly;\n"
                   " * \\endcode\n"
                   " */\n"
                   "class Foo {};\n";
  REQUIRE(rule.check(g, makeReadFile(src)).empty());
}

TEST_CASE("SF.7: using namespace after block comment closes — violation", "[rules][sf7]")
{
  DependencyGraph g;
  g.addNode("a.h");

  Sf7UsingNamespace rule;
  const auto src = "#pragma once\n"
                   "/* doc */\n"
                   "using namespace std;\n";
  const auto v = rule.check(g, makeReadFile(src));
  REQUIRE(v.size() == 1);
  CHECK(v[0].line == 3);
}
