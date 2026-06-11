#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>

#include "archcheck/config/config_loader.h"

namespace
{

std::filesystem::path fixture(const std::string &kind, const std::string &name)
{
  return std::filesystem::path(ARCHCHECK_FIXTURES_DIR) / "config" / kind / name / "archcheck.yml";
}

} // namespace

TEST_CASE("config loader: tiny — 2 modules + 1 forbidden", "[config][pass]")
{
  const auto cfg = archcheck::config::load(fixture("pass", "tiny"));
  REQUIRE(cfg.version == 1);
  REQUIRE(cfg.modules.size() == 2);
  REQUIRE(cfg.modules[0].name == "core");
  REQUIRE(cfg.modules[1].name == "ui");
  REQUIRE(cfg.rules.size() == 1);
  REQUIRE(std::holds_alternative<archcheck::config::ForbiddenRule>(cfg.rules[0]));
}

TEST_CASE("config loader: layered — 3 modules + layers rule", "[config][pass]")
{
  const auto cfg = archcheck::config::load(fixture("pass", "layered"));
  REQUIRE(cfg.modules.size() == 3);
  REQUIRE(cfg.rules.size() == 1);
  const auto &rule = std::get<archcheck::config::LayersRule>(cfg.rules[0]);
  REQUIRE(rule.name == "main-layering");
  REQUIRE(rule.layers == std::vector<std::string>{"app", "domain", "infra"});
}

TEST_CASE("config loader: legacy — only forbidden rules", "[config][pass]")
{
  const auto cfg = archcheck::config::load(fixture("pass", "legacy"));
  REQUIRE(cfg.rules.size() == 2);
  REQUIRE(std::holds_alternative<archcheck::config::ForbiddenRule>(cfg.rules[0]));
  REQUIRE(std::holds_alternative<archcheck::config::ForbiddenRule>(cfg.rules[1]));
}

TEST_CASE("config loader: mixed — all three rule types in one config", "[config][pass]")
{
  const auto cfg = archcheck::config::load(fixture("pass", "mixed"));
  REQUIRE(cfg.modules.size() == 5);
  REQUIRE(cfg.rules.size() == 3);
  REQUIRE(std::holds_alternative<archcheck::config::LayersRule>(cfg.rules[0]));
  REQUIRE(std::holds_alternative<archcheck::config::IndependenceRule>(cfg.rules[1]));
  REQUIRE(std::holds_alternative<archcheck::config::ForbiddenRule>(cfg.rules[2]));
}

TEST_CASE("config loader: thresholds — overrides chain_length and god_header_fan_in", "[config][pass]")
{
  const auto cfg = archcheck::config::load(fixture("pass", "thresholds"));
  REQUIRE(cfg.thresholds.chainLength == 7);
  REQUIRE(cfg.thresholds.godHeaderFanIn == 25);
}

TEST_CASE("config loader: thresholds — keep embedded defaults when block absent", "[config][pass]")
{
  const auto cfg = archcheck::config::load(fixture("pass", "tiny"));
  REQUIRE(cfg.thresholds.chainLength == 10);
  REQUIRE(cfg.thresholds.godHeaderFanIn == 50);
}

TEST_CASE("config loader: findConfig walks up to the nearest .archcheck.yml", "[config][pass]")
{
  const std::filesystem::path deep =
      std::filesystem::path(ARCHCHECK_FIXTURES_DIR) / "config" / "discovery" / "nested" / "deep";
  const auto found = archcheck::config::findConfig(deep);
  REQUIRE(found.has_value());
  REQUIRE(found->filename() == ".archcheck.yml");
  REQUIRE(found->parent_path().filename() == "discovery");
}

TEST_CASE("config loader: findConfig returns nullopt when no config up the tree", "[config][pass]")
{
  // Relies on no .archcheck.yml existing at the system temp dir or above.
  REQUIRE_FALSE(archcheck::config::findConfig(std::filesystem::temp_directory_path()).has_value());
}

using Catch::Matchers::ContainsSubstring;

TEST_CASE("config loader: rejects unknown top-level key", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_unknown_top_key")),
                      ContainsSubstring("unknown top-level key"));
}

TEST_CASE("config loader: rejects unknown threshold key", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_unknown_threshold_key")),
                      ContainsSubstring("unknown threshold key"));
}

TEST_CASE("config loader: rejects non-positive threshold", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_threshold_not_positive")),
                      ContainsSubstring("must be a positive integer"));
}

TEST_CASE("config loader: rejects unsupported schema version", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_wrong_version")),
                      ContainsSubstring("unsupported schema version"));
}

TEST_CASE("config loader: rejects duplicate module name", "[config][fail]")
{
  REQUIRE_THROWS_AS(archcheck::config::load(fixture("fail", "fail_duplicate_module")), archcheck::config::ConfigError);
}

TEST_CASE("config loader: rejects duplicate rule name", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_duplicate_rule_name")),
                      ContainsSubstring("duplicate rule name"));
}

TEST_CASE("config loader: rejects unknown rule type", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_unknown_rule_type")),
                      ContainsSubstring("unknown type"));
}

TEST_CASE("config loader: rejects rule referencing unknown module", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_unknown_module_in_rule")),
                      ContainsSubstring("unknown module"));
}

TEST_CASE("config loader: rejects forbidden from/to overlap", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_from_to_overlap")),
                      ContainsSubstring("both 'from' and 'to'"));
}

TEST_CASE("config loader: malformed YAML raises ConfigError, not abort", "[config][fail]")
{
  // ryml's default error handler abort()s the process; the loader must install
  // its own handler and translate the failure into the exit-2 ConfigError path.
  REQUIRE_THROWS_AS(archcheck::config::load(fixture("fail", "fail_malformed_yaml")), archcheck::config::ConfigError);
}

TEST_CASE("config loader: rejects empty layers list", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_empty_layers")),
                      ContainsSubstring("must be a non-empty list"));
}

TEST_CASE("config loader: rejects rule without name", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_missing_name")),
                      ContainsSubstring("'type' and 'name'"));
}
