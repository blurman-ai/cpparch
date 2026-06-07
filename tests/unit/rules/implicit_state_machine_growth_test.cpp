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

TEST_CASE("implicit_state_machine_growth: *Cache/*Settings struct excluded", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("cache_settings.h");

  // State-like names but the struct is a config bag → excluded by name.
  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct SettingsCache {
  bool started;
  bool running;
  bool paused;
  bool failed;
  bool completed;
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: vendored path is skipped", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("third_party/foo/state.h");

  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct Decoder {
  bool started;
  bool running;
  bool paused;
  bool failed;
  bool completed;
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: has_/use_ capability flags are not state", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("material.h");

  // has_*/use_* presence/capability flags → ratio below threshold → no violation.
  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct Material {
  bool has_clearcoat;
  bool has_transmission;
  bool has_volume;
  bool use_shaders;
  bool use_srgb;
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: local bools in method bodies are not counted", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("timer.h");

  // Only 2 real fields (started, running); the rest are locals inside methods.
  ImplicitStateMachineGrowth rule;
  REQUIRE(rule.check(g, makeReadFile(R"(
#pragma once
struct Timer {
  bool started;
  bool running;
  void tick() {
    bool result = false;
    bool success = true;
    bool done = result && success;
  }
  void reset() {
    bool ok = true;
  }
};
)")).empty());
}

TEST_CASE("implicit_state_machine_growth: state subset survives despite capability flags", "[rules][implicit_state_machine_growth]")
{
  DependencyGraph g;
  g.addNode("backend.h");

  // 5 fields, 3 state-like (started/running/stopped) + 2 capability → 60% ratio → flagged.
  ImplicitStateMachineGrowth rule;
  const auto v = rule.check(g, makeReadFile(R"(
#pragma once
struct Backend {
  bool started;
  bool running;
  bool stopped;
  bool has_cache;
  bool use_mmap;
};
)"));
  REQUIRE(v.size() == 1);
  CHECK(v[0].ruleId == "implicit_state_machine_growth");
}
