#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace archcheck::config
{

struct ModuleDef
{
  std::string name;
  std::vector<std::string> paths;
};

struct LayersRule
{
  std::string name;
  std::vector<std::string> layers;
};

struct IndependenceRule
{
  std::string name;
  std::vector<std::string> modules;
};

struct ForbiddenRule
{
  std::string name;
  std::vector<std::string> from;
  std::vector<std::string> to;
};

using Rule = std::variant<LayersRule, IndependenceRule, ForbiddenRule>;

// Tunable rule defaults, single source of truth. Overridable via the phase-2
// `thresholds:` block in .archcheck.yml (see docs/config_format.md).
struct Thresholds
{
  std::size_t chainLength = 10;    // Lakos.ChainLength: include chain depth
  std::size_t godHeaderFanIn = 50; // Lakos.GodHeader: header fan-in
  // --diff: skip the local-complexity advisory when the diff adds more lines
  // (bulk source imports are not authored evolution; #109 corpus calibration —
  // genuine large features stay below ~6k, bulk drops start at ~20k).
  std::size_t diffMaxAddedLines = 10000;
};

struct Config
{
  int version = 0;
  std::vector<ModuleDef> modules;
  std::vector<Rule> rules;
  Thresholds thresholds;
};

} // namespace archcheck::config
