#include <catch2/catch_test_macros.hpp>

#include "archcheck/git/history_query.h"
#include "archcheck/scan/defect_attractor.h"

using archcheck::git::CommitStats;
using archcheck::git::FileChange;
using archcheck::scan::DefectAttractorDetector;

TEST_CASE("DefectAttractorDetector: fix-like pattern matching")
{
  // Test word-boundary word matching: "fix", "fixed", "fixes" should match
  // but "prefix", "suffix", "postfix", "fixture" should NOT match.
  CommitStats fixCommit;
  fixCommit.sha = "abc123";
  fixCommit.subject = "fix: critical bug";
  fixCommit.files.push_back({"src/main.cpp", 5, 2});

  CommitStats prefixCommit;
  prefixCommit.sha = "def456";
  prefixCommit.subject = "prefix support for config"; // Should NOT match
  prefixCommit.files.push_back({"src/config.cpp", 10, 0});

  CommitStats suffixCommit;
  suffixCommit.sha = "ghi789";
  suffixCommit.subject = "update suffix handling"; // Should NOT match
  suffixCommit.files.push_back({"src/suffix.cpp", 8, 1});

  CommitStats fixtureCommit;
  fixtureCommit.sha = "jkl012";
  fixtureCommit.subject = "fixture: add test data"; // Should NOT match
  fixtureCommit.files.push_back({"tests/fixtures.cpp", 3, 0});

  std::vector<CommitStats> history = {fixtureCommit, suffixCommit, prefixCommit, fixCommit};
  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // Only the file in the actual "fix" commit should be counted.
  // But "tests/fixtures.cpp" is skipped as a test file, and others are not in top-decile.
  // With only 1 production file (src/main.cpp) having 1 fix touch, it's below floor (5).
  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: 6 fix commits to one file triggers finding")
{
  // Create 6 fix-like commits all touching one production file.
  std::vector<CommitStats> history;
  for (int i = 0; i < 6; ++i)
  {
    CommitStats c;
    c.sha = "commit" + std::to_string(i);
    c.subject = "fix: issue #" + std::to_string(i);
    c.files.push_back({"src/core.cpp", 5, 2});
    history.push_back(c);
  }

  // Add 6 other commits touching different files to have 12 total files with fixes
  for (int i = 6; i < 12; ++i)
  {
    CommitStats c;
    c.sha = "commit" + std::to_string(i);
    c.subject = "fix: issue #" + std::to_string(i);
    c.files.push_back({"src/other" + std::to_string(i - 6) + ".cpp", 1, 0});
    history.push_back(c);
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // src/core.cpp has 6 fix touches (>= 5), each other file has 1.
  // With 12 files with fixes, top-decile threshold = 90th percentile.
  // Sorted: [1, 1, 1, 1, 1, 1, 6] — 90th percentile = 1.8 → index 10.8 → 6
  // So only src/core.cpp with 6 touches >= 6 and >= 5 should qualify.
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].file == "src/core.cpp");
  REQUIRE(violations[0].ruleId == "HIST.1.defect_attractor");
  REQUIRE(violations[0].line == 0);
}

TEST_CASE("DefectAttractorDetector: 4 fix touches below floor (5) produces no finding")
{
  std::vector<CommitStats> history;
  for (int i = 0; i < 4; ++i)
  {
    CommitStats c;
    c.sha = "commit" + std::to_string(i);
    c.subject = "fix: issue #" + std::to_string(i);
    c.files.push_back({"src/main.cpp", 3, 1});
    history.push_back(c);
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: merge commits are skipped")
{
  std::vector<CommitStats> history;

  CommitStats mergeCommit;
  mergeCommit.sha = "merge123";
  mergeCommit.subject = "Merge pull request #42 from feature/xyz";
  mergeCommit.files.push_back({"src/main.cpp", 10, 5});
  history.push_back(mergeCommit);

  CommitStats fixCommit;
  fixCommit.sha = "fix456";
  fixCommit.subject = "fix: bug";
  fixCommit.files.push_back({"src/main.cpp", 2, 1});
  history.push_back(fixCommit);

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // Only 1 touch in the actual fix commit, below floor (5)
  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: mechanical fix (>30 files) is skipped")
{
  std::vector<CommitStats> history;

  CommitStats mechanicalFix;
  mechanicalFix.sha = "mech123";
  mechanicalFix.subject = "fix: lint warnings across codebase";
  // Add 40 files to trigger mechanical-fix skip
  for (int i = 0; i < 40; ++i)
  {
    mechanicalFix.files.push_back({"src/file" + std::to_string(i) + ".cpp", 1, 1});
  }
  history.push_back(mechanicalFix);

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // Mechanical fix should not contribute to any file's count
  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: vendor paths are excluded")
{
  std::vector<CommitStats> history;

  // 6 fix commits to a vendor file
  for (int i = 0; i < 6; ++i)
  {
    CommitStats c;
    c.sha = "commit" + std::to_string(i);
    c.subject = "fix: vendor issue #" + std::to_string(i);
    c.files.push_back({"third_party/zlib/compress.c", 5, 2});
    history.push_back(c);
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // Vendor files are excluded, so no findings
  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: test files are excluded")
{
  std::vector<CommitStats> history;

  // 6 fix commits to a test file
  for (int i = 0; i < 6; ++i)
  {
    CommitStats c;
    c.sha = "commit" + std::to_string(i);
    c.subject = "fix: test #" + std::to_string(i);
    c.files.push_back({"tests/test_core.cpp", 3, 1});
    history.push_back(c);
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // Test files are excluded, so no findings
  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: results sorted by fix touches descending")
{
  std::vector<CommitStats> history;

  // Create fix commits: file1 gets 8 touches, file2 gets 6 touches, file3 gets 5 touches
  for (int i = 0; i < 8; ++i)
  {
    CommitStats c;
    c.sha = "commit_file1_" + std::to_string(i);
    c.subject = "fix: issue";
    c.files.push_back({"src/file1.cpp", 1, 0});
    history.push_back(c);
  }

  for (int i = 0; i < 6; ++i)
  {
    CommitStats c;
    c.sha = "commit_file2_" + std::to_string(i);
    c.subject = "fix: bug";
    c.files.push_back({"src/file2.cpp", 1, 0});
    history.push_back(c);
  }

  for (int i = 0; i < 5; ++i)
  {
    CommitStats c;
    c.sha = "commit_file3_" + std::to_string(i);
    c.subject = "fix: problem";
    c.files.push_back({"src/file3.cpp", 1, 0});
    history.push_back(c);
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // With 3 files with fixes, top-decile = max = 8 touches
  // file1 (8 touches) >= 8 and >= 5: YES
  // file2 (6 touches) >= 8: NO
  // file3 (5 touches) >= 8: NO
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].file == "src/file1.cpp");
}

TEST_CASE("DefectAttractorDetector: different fix patterns recognized")
{
  std::vector<CommitStats> history;

  std::vector<std::string> subjects = {
      "fix: segmentation fault",     // "fix" and "segfault" keyword
      "bug: memory leak",            // "bug" and "leak" keyword
      "hotfix: crash on startup",    // "hotfix" and "crash" keyword
      "regress: undo broken change", // "regress" keyword
      "fault in exception handler",  // "fault" keyword
      "oops: typo in constant"       // "oops" keyword
  };

  for (int i = 0; i < static_cast<int>(subjects.size()); ++i)
  {
    CommitStats c;
    c.sha = "commit" + std::to_string(i);
    c.subject = subjects[i];
    c.files.push_back({"src/main.cpp", 2, 1});
    history.push_back(c);
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // src/main.cpp has 6 fix touches (>= 5), and is the only production file
  // so it's in top-1, which qualifies as top-decile
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].file == "src/main.cpp");
}

TEST_CASE("DefectAttractorDetector: empty history produces no findings")
{
  std::vector<CommitStats> history;
  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  REQUIRE(violations.empty());
}

TEST_CASE("DefectAttractorDetector: fewer than 10 files uses top-1 threshold")
{
  std::vector<CommitStats> history;

  // Create 5 files with varying fix touches: 5, 6, 7, 4, 3
  std::vector<std::pair<std::string, int>> files = {
      {"src/a.cpp", 5}, {"src/b.cpp", 6}, {"src/c.cpp", 7}, {"src/d.cpp", 4}, {"src/e.cpp", 3}};

  for (const auto &[path, touches] : files)
  {
    for (int i = 0; i < touches; ++i)
    {
      CommitStats c;
      c.sha = path + "_" + std::to_string(i);
      c.subject = "fix: issue";
      c.files.push_back({path, 1, 0});
      history.push_back(c);
    }
  }

  DefectAttractorDetector detector(history);
  const auto violations = detector.detect();

  // Top-1 threshold = 7 (max)
  // Only src/c.cpp with 7 touches should qualify
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].file == "src/c.cpp");
}
