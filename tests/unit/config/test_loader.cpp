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

using Catch::Matchers::ContainsSubstring;

TEST_CASE("config loader: rejects unknown top-level key", "[config][fail]")
{
  REQUIRE_THROWS_WITH(archcheck::config::load(fixture("fail", "fail_unknown_top_key")),
                      ContainsSubstring("unknown top-level key"));
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
