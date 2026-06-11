#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_map>

#include "archcheck/scan/duplication/fragmenter.h"
#include "archcheck/scan/duplication/similarity.h"
#include "archcheck/scan/duplication/token_normalizer.h"

using namespace archcheck::scan::duplication;

namespace
{
Fragment soleFragment(const std::string &src, const std::string &file)
{
  const auto tokens = lex(src, true);
  auto frags = extractFragments(tokens, src, file, FragmentOptions{});
  REQUIRE(frags.size() == 1);
  return frags.front();
}

const std::string kBase = "int run(int count)\n"
                          "{\n"
                          "  int alpha = count + 1;\n"
                          "  int beta = alpha * 2;\n"
                          "  int gamma = beta - alpha;\n"
                          "  int delta = gamma + beta;\n"
                          "  int epsilon = delta * gamma;\n"
                          "  return epsilon + alpha;\n"
                          "}\n";
const std::string kHeavyRename = "int run(int count)\n"
                                 "{\n"
                                 "  int aa = count + 1;\n"
                                 "  int bb = aa * 2;\n"
                                 "  int cc = bb - aa;\n"
                                 "  int dd = cc + bb;\n"
                                 "  int ee = dd * cc;\n"
                                 "  return ee + aa;\n"
                                 "}\n";
} // namespace

// A Type-2 clone (only local names renamed) is rename-blind at the TOKEN level by
// design (architecture-spec §4/§6): the normalized seq is identical, so lcs == 1 and
// weightedJaccard == 1. But lineOverlap compares RAW source lines, and a rename that
// touches every line drives it toward 0. The P0.6 joint floor (duplication_scanner.cpp
// phase8JointTokenOrderFloor) requires line >= 0.50 alongside weighted >= 0.75, so a
// heavily-renamed true clone is dropped — a deliberate precision/recall trade-off, NOT
// a recall bug. A light rename (few lines touched) keeps lineOverlap high and is
// reported as RENAMED (verified end-to-end: weighted=1, line=0.86).
TEST_CASE("Renamed clone: token metrics rename-blind, raw-line floor (P0.6) gates heavy renames",
          "[duplication][renamed]")
{
  const Fragment a = soleFragment(kBase, "a.c");
  const Fragment b = soleFragment(kHeavyRename, "b.c");
  const std::unordered_map<std::string, double> noIdf;

  SECTION("token level is rename-blind (the Type-2 guarantee)")
  {
    REQUIRE(a.seq == b.seq);
    REQUIRE(lcsRatio(a, b) == 1.0);
    REQUIRE(weightedJaccard(a, b, noIdf) == 1.0);
  }

  SECTION("raw-line overlap collapses under a whole-body rename (below P0.6 floor)")
  {
    REQUIRE(lineOverlap(a, b) < 0.50);
  }
}
