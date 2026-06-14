#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/satd_scan.h"

namespace
{

TEST_CASE("satd_scan: SATD.1 detects TODO marker", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 1, "  int x = 1; // TODO: optimize this\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
  REQUIRE(violations[0].file == "file.cpp");
  REQUIRE(violations[0].line == 1);
}

TEST_CASE("satd_scan: SATD.2 detects FIXME without issue id (block comment)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 2, "  /* FIXME: broken logic */ return 0;\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.2");
}

TEST_CASE("satd_scan: SATD.2 detects HACK without issue id (line comment)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 3, "  void func() { // HACK: temporary\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.2");
}

TEST_CASE("satd_scan: SATD.1 detects XXX marker", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 4, "  x = y; // XXX: investigate this\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: SATD.1 detects TEMP marker", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 5, "  // TEMP: for testing\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: SATD.1 detects temporary (case-insensitive)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 6, "  // temporary solution\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: SATD.1 detects workaround (case-insensitive)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 7, "  // Workaround for GCC bug\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: SATD.1 detects dirty (case-insensitive)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 8, "  // DIRTY: needs cleanup\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: SATD.1 detects quick fix (case-insensitive)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 9, "  // quick-fix: remove later\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: SATD.2 detects FIXME without issue id", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 10, "  // FIXME: handle error case\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.2");
}

TEST_CASE("satd_scan: SATD.2 detects HACK without issue id", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 11, "  // HACK: temporary fix\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.2");
}

TEST_CASE("satd_scan: HACK with JIRA id is not SATD.2", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 12, "  // HACK PROJ-123: temporary fix\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1"); // Only SATD.1, not SATD.2
}

TEST_CASE("satd_scan: HACK with GitHub issue id is not SATD.2", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 13, "  // HACK #456: temporary fix\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1"); // Only SATD.1, not SATD.2
}

TEST_CASE("satd_scan: FIXME with gh/issue prefix is not SATD.2", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 14, "  // FIXME gh-789: broken\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1"); // Only SATD.1, not SATD.2
}

TEST_CASE("satd_scan: lowercase todo in comment is not a marker", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 15, "  // remember to todo the dishes\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.empty());
}

TEST_CASE("satd_scan: dirty in identifier is not a marker", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 16, "  struct DirtyBuffer { int x; };\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.empty());
}

TEST_CASE("satd_scan: marker in code (no comment) is ignored", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 17, "  std::string s = \"TODO\"; // no marker here\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.empty()); // The TODO inside quotes is not in a comment
}

TEST_CASE("satd_scan: block comment with marker", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 18, "  /* TODO: fix this */ int x = 1;\n"},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].ruleId == "SATD.1");
}

TEST_CASE("satd_scan: multiple violations per diff (one per line)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{
      {"file.cpp", 1, "  // TODO: one\n"},
      {"file.cpp", 2, "  // FIXME: two\n"},
      {"file.cpp", 3, "  // no marker here\n"},
      {"file.cpp", 4, "  // HACK: three\n"},
  };

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 3);
  REQUIRE(violations[0].ruleId == "SATD.1");
  REQUIRE(violations[1].ruleId == "SATD.2");
  REQUIRE(violations[2].ruleId == "SATD.2");
}

TEST_CASE("satd_scan: message truncation at 120 chars", "[scan][satd]")
{
  std::string longLine = "  // TODO: ";
  longLine += std::string(200, 'x');
  longLine += "\n";

  const std::vector<archcheck::git::AddedLine> lines{{
      {"file.cpp", 1, longLine},
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].message.size() <= 120);
}

TEST_CASE("satd_scan: vendored and test code are skipped, project code still fires (#124)", "[scan][satd]")
{
  const std::vector<archcheck::git::AddedLine> lines{{
      {"ThirdParty/imgui/imgui.cpp", 1, "  // FIXME: vendored debt, not ours\n"}, // vendored dir
      {"vendor/foo.c", 2, "  // TODO: also vendored\n"},                          // vendored dir
      {"src/json.hpp", 3, "  // FIXME: bundled single-file lib\n"},               // vendored basename
      {"tests/widget_test.cpp", 4, "  // FIXME: test debt\n"},                    // test dir + basename
      {"src/app.cpp", 5, "  // FIXME: our real debt\n"},                          // project code
  }};

  const auto violations = archcheck::scan::detectSatdMarkers(lines);
  // Only the production project line is reported; vendored and test TODO/FIXME is
  // not the project's production self-admitted debt.
  REQUIRE(violations.size() == 1);
  REQUIRE(violations[0].file == "src/app.cpp");
}

} // namespace
