#include "archcheck/scan/duplication/clone_index.h"
#include "archcheck/scan/duplication/fragmenter.h"

#include <catch2/catch_test_macros.hpp>

using namespace archcheck::scan::duplication;

TEST_CASE("CloneIndex: empty fragments", "[duplication]")
{
  std::vector<Fragment> frags;
  const auto idx = buildIndex(frags);

  REQUIRE(idx.df.empty());
  REQUIRE(idx.idf.empty());
  REQUIRE(idx.postings.empty());
  REQUIRE(idx.sharedRare.empty());
}

TEST_CASE("CloneIndex: single fragment", "[duplication]")
{
  Fragment f;
  f.bag = {{"id", 3}, {"lit", 2}, {"+", 1}};
  f.diversity = 0.8;

  std::vector<Fragment> frags = {f};
  const auto idx = buildIndex(frags);

  REQUIRE(idx.df.size() == 3);
  REQUIRE(idx.idf.size() == 3);
  REQUIRE(idx.sharedRare.empty());  // no pairs possible
}

TEST_CASE("CloneIndex: two identical fragments", "[duplication]")
{
  Fragment f1, f2;
  f1.bag = {{"id", 2}, {"lit", 1}};
  f1.diversity = 1.0;
  f2.bag = {{"id", 2}, {"lit", 1}};
  f2.diversity = 1.0;

  std::vector<Fragment> frags = {f1, f2};
  const auto idx = buildIndex(frags);

  REQUIRE(idx.sharedRare.size() > 0);  // should find shared tokens
}

TEST_CASE("CloneIndex: rare token filtering", "[duplication]")
{
  Fragment f1, f2;
  f1.bag = {{"rare", 1}, {"common", 50}};  // rare appears in 1 fragment
  f1.diversity = 1.0;
  f2.bag = {{"rare", 1}, {"common", 50}};
  f2.diversity = 1.0;

  std::vector<Fragment> frags = {f1, f2};
  IndexOptions opts;
  opts.rareDfCap = 2;  // tokens with df <= 2 are "rare"

  const auto idx = buildIndex(frags, opts);

  REQUIRE(idx.df.at("rare") == 2);   // rare appears in 2 fragments
  REQUIRE(idx.df.at("common") == 2);  // common also appears in both
  // Both are <= 2, so both go into postings (with this cap)
}

TEST_CASE("CloneIndex: minDiversity filter", "[duplication]")
{
  Fragment f1, f2;
  f1.bag = {{"id", 1}};
  f1.diversity = 0.3;  // skeletal
  f2.bag = {{"id", 1}};
  f2.diversity = 0.3;  // skeletal

  std::vector<Fragment> frags = {f1, f2};
  IndexOptions opts;
  opts.minDiversity = 0.5;  // filter out low-diversity pairs

  const auto idx = buildIndex(frags, opts);

  REQUIRE(idx.sharedRare.empty());  // filtered out due to low diversity
}

TEST_CASE("CloneIndex: minSharedRare filter", "[duplication]")
{
  Fragment f1, f2;
  f1.bag = {{"id", 1}, {"x", 1}};  // 2 rare tokens
  f1.diversity = 1.0;
  f2.bag = {{"id", 1}, {"y", 1}};  // shares only 1 rare token
  f2.diversity = 1.0;

  std::vector<Fragment> frags = {f1, f2};
  IndexOptions opts;
  opts.minSharedRare = 2;  // require >= 2 shared rare tokens

  const auto idx = buildIndex(frags, opts);

  REQUIRE(idx.sharedRare.empty());  // only 1 shared token, filtered
}

TEST_CASE("CloneIndex: idf weights computed", "[duplication]")
{
  Fragment f1, f2;
  f1.bag = {{"rare_token", 1}};
  f1.diversity = 1.0;
  f2.bag = {{"rare_token", 1}};
  f2.diversity = 1.0;

  std::vector<Fragment> frags = {f1, f2};
  const auto idx = buildIndex(frags);

  // IDF for a token appearing in 2/2 documents should be log(2/2) = log(1) = 0
  REQUIRE(idx.idf.count("rare_token") > 0);
}
