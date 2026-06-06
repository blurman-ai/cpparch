#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "archcheck/graph/baseline.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"
#include "archcheck/rules/rule_set.h"
#include "archcheck/rules/violation.h"
#include "archcheck/scan/include_resolver.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"

using archcheck::graph::DependencyGraph;
using archcheck::graph::loadBaseline;
using archcheck::graph::NodeId;
using archcheck::rules::makeDriftRuleSet;
using archcheck::rules::ViolationList;
using archcheck::scan::Resolution;
using archcheck::scan::resolveIncludes;
using archcheck::scan::scanIncludes;

namespace
{

std::filesystem::path drift_fixture(std::string_view name)
{
  return std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / name;
}

std::string read_file(const std::filesystem::path &p)
{
  std::ifstream f(p, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

DependencyGraph build_graph_from_dir(const std::filesystem::path &root)
{
  const auto files = archcheck::scan::discoverFiles(root);
  const auto index = archcheck::scan::buildProjectIndex(files);
  DependencyGraph g;
  std::vector<NodeId> id_map;
  id_map.reserve(files.size());
  for (const auto &f : files)
    id_map.push_back(g.addNode(f.path));
  for (std::size_t i = 0; i < files.size(); ++i)
  {
    const auto resolved =
        resolveIncludes(scanIncludes(read_file(root / files[i].path)).directives, files[i].path, files, index);
    for (const auto &r : resolved)
    {
      if (r.resolution == Resolution::Project)
        g.addEdge(id_map[i], id_map[r.target]);
    }
  }
  return g;
}

ViolationList run_drift_check(std::string_view fixture_name)
{
  const auto root = drift_fixture(fixture_name);

  std::ifstream bfin(root / "baseline.graph.yml");
  REQUIRE(bfin.is_open());
  auto [baseline, err] = loadBaseline(bfin);
  REQUIRE(!err);

  const auto current = build_graph_from_dir(root);
  auto dummy_read = [](std::string_view) { return std::string{}; };
  ViolationList all;
  for (const auto &rule : makeDriftRuleSet(std::move(baseline)))
  {
    auto v = rule->check(current, dummy_read);
    all.insert(all.end(), v.begin(), v.end());
  }
  return all;
}

} // namespace

TEST_CASE("drift fixture: shortcut_edge/pass — no violations", "[drift][fixtures]")
{
  REQUIRE(run_drift_check("drift_shortcut_edge/pass").empty());
}

TEST_CASE("drift fixture: shortcut_edge/fail_new_coupling — DRIFT.1 fires", "[drift][fixtures]")
{
  const auto v = run_drift_check("drift_shortcut_edge/fail_new_coupling");
  REQUIRE(v.size() == 1);
  REQUIRE(v[0].ruleId == "DRIFT.1");
}

TEST_CASE("drift fixture: cycle_growth/pass — no violations", "[drift][fixtures]")
{
  REQUIRE(run_drift_check("drift_cycle_growth/pass").empty());
}

TEST_CASE("drift fixture: cycle_growth/fail_new_cycle — DRIFT.2 fires", "[drift][fixtures]")
{
  const auto v = run_drift_check("drift_cycle_growth/fail_new_cycle");
  // b.h -> a.h is also a shortcut (DRIFT.1) and creates a new cycle (DRIFT.2)
  const auto drift2 = std::count_if(v.begin(), v.end(), [](const auto &x) { return x.ruleId == "DRIFT.2"; });
  REQUIRE(drift2 == 1);
}

TEST_CASE("drift fixture: real_world/libresprite_pr581 — DRIFT.1 fires on toolbar -> preferences", "[drift][fixtures]")
{
  const auto v = run_drift_check("drift_real_world/libresprite_pr581");
  REQUIRE(v.size() == 1);
  REQUIRE(v[0].ruleId == "DRIFT.1");
  REQUIRE(v[0].message.find("app/ui/toolbar.cpp -> app/pref/preferences.h") != std::string::npos);
}

namespace
{
std::size_t count_rule(const ViolationList &v, std::string_view id)
{
  return static_cast<std::size_t>(std::count_if(v.begin(), v.end(), [&](const auto &x) { return x.ruleId == id; }));
}
} // namespace

TEST_CASE("drift fixture: bidirectional/fail_new_coupling — DRIFT.3 fires on core<->ui", "[drift][fixtures]")
{
  const auto v = run_drift_check("drift_bidirectional/fail_new_coupling");
  REQUIRE(count_rule(v, "DRIFT.3") == 1);
  const auto it = std::find_if(v.begin(), v.end(), [](const auto &x) { return x.ruleId == "DRIFT.3"; });
  REQUIRE(it != v.end());
  REQUIRE(it->message.find("'core' <-> 'ui'") != std::string::npos);
}

TEST_CASE("drift fixture: bidirectional/pass_one_directional — DRIFT.3 silent (one-way)", "[drift][fixtures]")
{
  REQUIRE(count_rule(run_drift_check("drift_bidirectional/pass_one_directional"), "DRIFT.3") == 0);
}

TEST_CASE("drift fixture: bidirectional/pass_file_cycle — DRIFT.3 defers to DRIFT.2", "[drift][fixtures]")
{
  const auto v = run_drift_check("drift_bidirectional/pass_file_cycle");
  REQUIRE(count_rule(v, "DRIFT.3") == 0); // direct two-file cycle is not DRIFT.3's job
  REQUIRE(count_rule(v, "DRIFT.2") >= 1); // ... it belongs to DRIFT.2
}
