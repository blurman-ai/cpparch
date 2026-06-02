#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/duplication/clone_classifier.h"

using namespace archcheck::scan::duplication;

TEST_CASE("Clone type: EXACT identical", "[duplication]")
{
  Fragment a, b;
  a.seq = {"int", "x", "=", "lit", ";"};
  b.seq = {"int", "x", "=", "lit", ";"};
  a.rawSeq = {"int", "x", "=", "5", ";"};
  b.rawSeq = {"int", "x", "=", "5", ";"};

  const char *type = cloneType(a, b);
  REQUIRE(std::string(type) == "EXACT");
}

TEST_CASE("Clone type: RENAMED identifiers differ", "[duplication]")
{
  Fragment a, b;
  a.seq = {"int", "id", "=", "lit", ";"};
  b.seq = {"int", "id", "=", "lit", ";"};
  a.rawSeq = {"int", "x", "=", "5", ";"};
  b.rawSeq = {"int", "y", "=", "5", ";"};

  const char *type = cloneType(a, b);
  REQUIRE(std::string(type) == "RENAMED");
}

TEST_CASE("Clone type: LITERAL numbers differ", "[duplication]")
{
  Fragment a, b;
  a.seq = {"int", "x", "=", "lit", ";"};
  b.seq = {"int", "x", "=", "lit", ";"};
  a.rawSeq = {"int", "x", "=", "5", ";"};
  b.rawSeq = {"int", "x", "=", "10", ";"};

  const char *type = cloneType(a, b);
  REQUIRE(std::string(type) == "LITERAL");
}

TEST_CASE("Clone type: STRUCTURAL normalized streams differ", "[duplication]")
{
  Fragment a, b;
  a.seq = {"int", "x", "=", "id", "(", ")", ";"};
  b.seq = {"int", "x", "=", "+", "lit", ";"};

  const char *type = cloneType(a, b);
  REQUIRE(std::string(type) == "STRUCTURAL");
}

TEST_CASE("DiffOp: identical sequences produce only equals", "[duplication]")
{
  std::vector<std::string> a = {"id", "=", "lit"};
  std::vector<std::string> b = {"id", "=", "lit"};

  const auto ops = diffTokens(a, b);
  REQUIRE(ops.size() == 3);
  for (const auto &op : ops)
  {
    REQUIRE(op.tag == '=');
  }
}

TEST_CASE("DiffOp: different sequences include changes", "[duplication]")
{
  std::vector<std::string> a = {"id", "=", "lit"};
  std::vector<std::string> b = {"id", "=", "+"};

  const auto ops = diffTokens(a, b);
  bool hasChange = false;
  for (const auto &op : ops)
  {
    if (op.tag == '~' || op.tag == '+' || op.tag == '-')
    {
      hasChange = true;
      break;
    }
  }
  REQUIRE(hasChange);
}
