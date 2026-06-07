#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/implicit_state_machine_growth.h"

using archcheck::graph::DependencyGraph;
using archcheck::rules::ImplicitStateMachineGrowth;

static auto makeReadFile(std::string content)
{
  return [c = std::move(content)](std::string_view) { return c; };
}

TEST_CASE("implicit_state_machine_growth: 5 state-like bools → violation", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("download.h");

  ImplicitStateMachineGrowth rule;
  const auto v = rule.check(g, makeReadFile(R"(
#pragma once
struct DownloadState {
  bool started;
  bool running;
  bool paused;
  bool failed;
  bool completed;
};
)"));
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "implicit_state_machine_growth");
  CHECK(v[0].file == "download.h");
}

TEST_CASE("implicit_state_machine_growth: 4 bools under threshold → no violation", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("small.h");

  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct SmallState {
  bool started;
  bool running;
  bool paused;
  bool completed;
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: 5 config-like bools → no violation", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("config.h");

  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct LoggerConfig {
  bool enable_console;
  bool enable_file;
  bool enable_syslog;
  bool verbose;
  bool debug;
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: .cpp file is not checked", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("main.cpp");

  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
struct StateX {
  bool a, b, c, d, e;
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: excluded struct pattern → no violation", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("options.h");

  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct ChordOptions {
  bool dim;
  bool min;
  bool maj;
  bool sus;
  bool extended;
};
)")).empty());
}
