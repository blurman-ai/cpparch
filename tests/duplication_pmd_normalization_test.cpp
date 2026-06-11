#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "archcheck/scan/duplication/token_normalizer.h"

using namespace archcheck::scan::duplication;

namespace
{
std::string readFixture(std::string_view name)
{
  const auto p = std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / "duplication" / "normalization" / name;
  std::ifstream in(p);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<std::string> normalizedSeq(std::string_view name)
{
  std::vector<std::string> seq;
  for (const auto &t : lex(readFixture(name), true))
  {
    seq.push_back(t.sym);
  }
  return seq;
}
} // namespace

// Known-answer fixtures adapted from PMD CPD testdata (BSD-3, see ATTRIBUTION.md).
// PMD's .txt expectations describe THEIR tokenizer; these cases pin OUR selective
// normalization semantics instead (duplication_architecture.md §3.1/§6).

TEST_CASE("PMD literals: only literal values differ -> normalized seqs match", "[duplication][pmd-fixtures]")
{
  const auto a = normalizedSeq("literals_a.cpp");
  REQUIRE(a == normalizedSeq("literals_b.cpp"));

  // The zoo must survive lexing as "lit" tokens: 5 char forms, 2 strings, the raw
  // string as ONE token, 3 separator numbers, 2 binary — 13 in total.
  std::size_t lits = 0;
  for (const auto &s : a)
  {
    if (s == "lit")
    {
      ++lits;
    }
  }
  REQUIRE(lits == 13);
}

TEST_CASE("PMD ignoreIdents: only local names differ -> normalized seqs match", "[duplication][pmd-fixtures]")
{
  REQUIRE(normalizedSeq("idents_a.cpp") == normalizedSeq("idents_b.cpp"));
}

TEST_CASE("Selective normalization: renamed CALLEE must change the seq", "[duplication][pmd-fixtures]")
{
  // Unlike PMD's blanket ignore-identifiers, callee names are a distinguishing
  // signal we deliberately keep: compute(...) vs process(...) are different code.
  REQUIRE(normalizedSeq("idents_a.cpp") != normalizedSeq("idents_c.cpp"));
}
