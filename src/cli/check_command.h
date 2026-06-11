#pragma once

#include <filesystem>
#include <optional>

#include "archcheck/config/config.h"

namespace archcheck::cli
{

enum class OutputFormat
{
  Text,
  Json
};

enum class BaselineMode
{
  None,
  Load,
  Save
};

struct BaselineOpts
{
  BaselineMode mode = BaselineMode::None;
  std::filesystem::path file;
  std::optional<std::filesystem::path> driftFile; // --drift-baseline <file>
};

// Run all default rules on `root` and report. Owns the baseline/drift policy:
// --save-baseline / --baseline filtering and the DRIFT.1/DRIFT.2 gating exit.
// Discovers .archcheck.yml from `root` unless an explicit `config` is given.
int runCheck(const std::filesystem::path &root, OutputFormat fmt, BaselineOpts baseline = {},
             std::optional<config::Config> config = std::nullopt);

// Save the include-graph snapshot of `root` for later --drift-baseline runs.
int runSaveGraphBaseline(const std::filesystem::path &root, const std::filesystem::path &file);

} // namespace archcheck::cli
