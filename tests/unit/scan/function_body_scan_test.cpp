#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/duplication/token_normalizer.h"
#include "archcheck/scan/function_body_scan.h"

namespace
{

std::vector<archcheck::scan::FunctionSpan> discover(const std::string &src)
{
  return archcheck::scan::discoverFunctions(archcheck::scan::duplication::lex(src));
}

TEST_CASE("function_body_scan: free function with arity", "[scan][complexity]")
{
  const auto spans = discover("int add(int a, int b) { return a + b; }");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "add");
  REQUIRE(spans[0].paramArity == 2);
}

TEST_CASE("function_body_scan: inline method inside class body", "[scan][complexity]")
{
  const auto spans = discover("class Foo\n{\n  int bar() const { return 1; }\n};\n");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "bar");
  REQUIRE(spans[0].paramArity == 0);
}

TEST_CASE("function_body_scan: out-of-class method keeps qualifier", "[scan][complexity]")
{
  const auto spans = discover("int Foo::bar(int x)\n{\n  return x;\n}\n");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "Foo::bar");
  REQUIRE(spans[0].paramArity == 1);
}

TEST_CASE("function_body_scan: operator() definition", "[scan][complexity]")
{
  const auto spans = discover("struct F\n{\n  int operator()(int x) const { return x; }\n};\n");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "operator()");
  REQUIRE(spans[0].paramArity == 1);
}

TEST_CASE("function_body_scan: overloads distinguished by arity", "[scan][complexity]")
{
  const auto spans = discover("int f(int a) { return a; }\nint f(int a, int b) { return a + b; }\n");
  REQUIRE(spans.size() == 2);
  REQUIRE(spans[0].paramArity == 1);
  REQUIRE(spans[1].paramArity == 2);
}

TEST_CASE("function_body_scan: constructor with init list and braced init", "[scan][complexity]")
{
  const auto spans = discover("Foo::Foo(int x) : a_(x), b_{x}\n{\n  run();\n}\n");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "Foo::Foo");
  REQUIRE(spans[0].paramArity == 1);
}

TEST_CASE("function_body_scan: destructor and template function", "[scan][complexity]")
{
  const auto spans = discover("Foo::~Foo() { close(); }\ntemplate <typename T> T biggest(T a, T b) { return a; }\n");
  REQUIRE(spans.size() == 2);
  REQUIRE(spans[0].qualifiedName == "Foo::~Foo");
  REQUIRE(spans[1].qualifiedName == "biggest");
  REQUIRE(spans[1].paramArity == 2);
}

TEST_CASE("function_body_scan: trailing return type", "[scan][complexity]")
{
  const auto spans = discover("auto h(int x) -> int { return x; }");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "h");
}

TEST_CASE("function_body_scan: declarations and control headers are not functions", "[scan][complexity]")
{
  const auto spans = discover("int f(int);\nvoid g()\n{\n  if (1) { }\n  while (0) { }\n  switch (2) { }\n}\n");
  REQUIRE(spans.size() == 1);
  REQUIRE(spans[0].qualifiedName == "g");
  REQUIRE(spans[0].paramArity == 0);
}

TEST_CASE("function_body_scan: body token range covers the braces", "[scan][complexity]")
{
  const auto src = std::string{"int one() { return 1; }"};
  const auto spans = discover(src);
  const auto tokens = archcheck::scan::duplication::lex(src);
  REQUIRE(spans.size() == 1);
  REQUIRE(tokens[spans[0].bodyBegin].sym == "{");
  REQUIRE(tokens[spans[0].bodyEnd].sym == "}");
}

} // namespace
