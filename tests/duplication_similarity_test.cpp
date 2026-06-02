#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "archcheck/scan/duplication/similarity.h"

using namespace archcheck::scan::duplication;

TEST_CASE("Similarity: plainJaccard identical fragments", "[duplication]")
{
  Fragment a, b;
  a.bag = {{"id", 2}, {"lit", 1}, {"+", 1}};
  b.bag = {{"id", 2}, {"lit", 1}, {"+", 1}};

  const double sim = plainJaccard(a, b);
  REQUIRE(sim == 1.0);
}

TEST_CASE("Similarity: plainJaccard disjoint fragments", "[duplication]")
{
  Fragment a, b;
  a.bag = {{"id", 1}, {"lit", 1}};
  b.bag = {{"+", 1}, {"-", 1}};

  const double sim = plainJaccard(a, b);
  REQUIRE(sim == 0.0);
}

TEST_CASE("Similarity: plainJaccard partial overlap", "[duplication]")
{
  Fragment a, b;
  a.bag = {{"id", 2}, {"lit", 1}}; // total: 3
  b.bag = {{"id", 1}, {"+", 1}};   // total: 2

  const double sim = plainJaccard(a, b);
  REQUIRE(std::abs(sim - 0.25) < 0.001);
}

TEST_CASE("Similarity: lcsLength simple match", "[duplication]")
{
  std::vector<std::string> a = {"id", "=", "lit"};
  std::vector<std::string> b = {"id", "=", "lit"};

  const std::size_t len = lcsLength(a, b);
  REQUIRE(len == 3);
}

TEST_CASE("Similarity: lcsLength no match", "[duplication]")
{
  std::vector<std::string> a = {"id", "lit"};
  std::vector<std::string> b = {"+", "-"};

  const std::size_t len = lcsLength(a, b);
  REQUIRE(len == 0);
}

TEST_CASE("Similarity: lcsRatio", "[duplication]")
{
  Fragment a, b;
  a.seq = {"id", "=", "lit", ";"};
  b.seq = {"id", "=", "lit", ";"};

  const double ratio = lcsRatio(a, b);
  REQUIRE(ratio == 1.0);
}
