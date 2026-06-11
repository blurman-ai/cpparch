#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>
#include <vector>

#include "archcheck/git/history_query.h"
#include "archcheck/scan/god_file_growth.h"

namespace
{

TEST_CASE("god_file_growth: all four conditions met → finding", "[scan]")
{
  // Create a file with steady growth: 5 consecutive commits with added > deleted
  std::vector<archcheck::git::CommitStats> history;
  for (int i = 0; i < 5; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "src/growing.cpp";
    fc.added = 50; // 5 commits * (50-10) = 200 net growth
    fc.deleted = 10;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  // Current LOC: 200 net, plus P75 calculation.
  // Files: [growing=200, small=100, tiny=50, medium=500]
  // Sorted: [50, 100, 200, 500], P75 idx=2 → P75=200
  // growing.cpp: LOC=200 >= P75=200 ✓
  // net growth: 200 >= +30%*200=60 ✓ (OR >= 300)
  // consecutive: 5 >= 5 ✓
  // no shrink ✓
  std::map<std::string, std::int32_t> currentLoc;
  currentLoc["src/growing.cpp"] = 200;
  currentLoc["src/small.cpp"] = 100;
  currentLoc["src/tiny.cpp"] = 50;
  currentLoc["src/medium.cpp"] = 500;

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SIZE.1.god_file_growth");
  REQUIRE(violations[0].file == "src/growing.cpp");
  REQUIRE(violations[0].line == 0);
}

TEST_CASE("god_file_growth: only 4 consecutive growth commits → no finding", "[scan]")
{
  // 4 consecutive growth commits (below threshold of 5)
  std::vector<archcheck::git::CommitStats> history;
  for (int i = 0; i < 4; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "src/growing.cpp";
    fc.added = 50;
    fc.deleted = 10;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  std::map<std::string, std::int32_t> currentLoc;
  currentLoc["src/growing.cpp"] = 1000;

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

TEST_CASE("god_file_growth: meaningful shrink present → no finding", "[scan]")
{
  // 5 consecutive growth commits followed by a shrink
  std::vector<archcheck::git::CommitStats> history;
  for (int i = 0; i < 5; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "src/growing.cpp";
    fc.added = 50;
    fc.deleted = 10;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  // Add a shrink commit with deleted - added >= 50
  archcheck::git::CommitStats shrinkCommit;
  shrinkCommit.sha = "shrink";
  shrinkCommit.subject = "Refactor";
  archcheck::git::FileChange fc;
  fc.path = "src/growing.cpp";
  fc.added = 10;
  fc.deleted = 60; // deleted - added = 50 >= threshold
  shrinkCommit.files.push_back(fc);
  history.push_back(shrinkCommit);

  std::map<std::string, std::int32_t> currentLoc;
  currentLoc["src/growing.cpp"] = 1000;

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

TEST_CASE("god_file_growth: growth +25% and +250 lines (below both thresholds) → no finding", "[scan]")
{
  // Net growth: 200 lines on a 1000 LOC file = +20% (below 30%)
  // Also below +300 absolute threshold
  std::vector<archcheck::git::CommitStats> history;
  for (int i = 0; i < 5; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "src/growing.cpp";
    fc.added = 40;
    fc.deleted = 0;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  // Total: 200 added, 0 deleted on a 1000 LOC file = +20% (below 30%)
  std::map<std::string, std::int32_t> currentLoc;
  currentLoc["src/growing.cpp"] = 1000;

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

TEST_CASE("god_file_growth: small file below P75 → no finding", "[scan]")
{
  // Small file that doesn't meet the size criterion
  // Net growth: 5 * 500 = 2500 lines (meets +300 absolute threshold)
  // But file is small, so it won't meet P75 criterion
  std::vector<archcheck::git::CommitStats> history;
  for (int i = 0; i < 5; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "src/small.cpp";
    fc.added = 500;
    fc.deleted = 0;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  std::map<std::string, std::int32_t> currentLoc;
  // Small files only: 50, 40, 30
  // Sorted: [30, 40, 50], P75 idx = ceil(3*0.75)-1 = 2 → P75=50
  // small.cpp has LOC=50 which is exactly P75, not strictly below
  // But let's check: currentLoc should be net+initial. If starts at 0, then 2500.
  // But that's huge! More realistic: starts at some value, grows by 2500.
  // Let's say starts at 10, now 2510. Still huge.
  // To avoid false positive, let's make currentLoc = 50 explicitly.
  // With P75=50, 50 >= 50 is true, but net growth = 2500 >= +300, so it WOULD fire.
  // Let's make the files different: [20, 30, 50]
  // Then P75 idx = 2, P75 = 50. File = 50, meets criterion 1.
  // This test should NOT trigger, so let's make sure we fail on another criterion.
  // Let's make: [20, 30, 40], so P75=40. File small.cpp=50, so 50 >= 40 ✓,
  // but then it WOULD trigger on growth 2500.
  // Actually, the intent of this test is to show a file that's big but doesn't grow much.
  // Let me re-read the test name: "small file below P75"
  // So currentLoc should be small (below P75), not the net growth.
  currentLoc["src/small.cpp"] = 50;
  currentLoc["src/tiny.cpp"] = 40;
  currentLoc["src/tinier.cpp"] = 30;
  // Sorted: [30, 40, 50], P75 idx = ceil(3*0.75)-1 = 2 → P75=50
  // small.cpp = 50, which is exactly P75, not strictly below.
  // To make it truly below, let's adjust:
  // currentLoc["src/small.cpp"] = 49;
  // Then P75 would be different. Let me recalculate for [30, 40, 49]:
  // Sorted: [30, 40, 49], P75 idx = 2 → P75=49
  // That doesn't help.
  // Let's just make the corpus bigger so P75 is higher.
  // Files: [30, 40, 50, 100, 200]
  // Sorted: [30, 40, 50, 100, 200], P75 idx = ceil(5*0.75)-1 = ceil(3.75)-1 = 4-1 = 3 → P75=100
  // Now small.cpp=50 < 100, so it fails criterion 1. ✓
  currentLoc.clear();
  currentLoc["src/small.cpp"] = 50;
  currentLoc["src/tiny.cpp"] = 40;
  currentLoc["src/tinier.cpp"] = 30;
  currentLoc["src/medium.cpp"] = 100;
  currentLoc["src/large.cpp"] = 200;

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

TEST_CASE("god_file_growth: vendor path excluded", "[scan]")
{
  // File in vendor directory
  std::vector<archcheck::git::CommitStats> history;
  for (int i = 0; i < 5; ++i)
  {
    archcheck::git::CommitStats commit;
    commit.sha = "abc" + std::to_string(i);
    commit.subject = "Growth " + std::to_string(i);
    archcheck::git::FileChange fc;
    fc.path = "third_party/lib.cpp";
    fc.added = 50;
    fc.deleted = 10;
    commit.files.push_back(fc);
    history.push_back(commit);
  }

  std::map<std::string, std::int32_t> currentLoc;
  currentLoc["third_party/lib.cpp"] = 1000;

  archcheck::scan::GodFileGrowthDetector detector(currentLoc, history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

} // namespace
