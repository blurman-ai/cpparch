#include "cli/check_command.h"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "archcheck/config/config_loader.h"
#include "archcheck/graph/baseline.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/report/json_reporter.h"
#include "archcheck/report/text_reporter.h"
#include "archcheck/report/violation_baseline.h"
#include "archcheck/rules/rule_set.h"
#include "archcheck/scan/disk_file_source.h"

namespace archcheck::cli
{

namespace
{

// Returns 0 on success, 2 on I/O error (already printed to stderr).
int trySaveBaseline(const archcheck::rules::ViolationList &all, const std::filesystem::path &file)
{
  try
  {
    archcheck::report::saveBaseline({all}, file);
  }
  catch (const archcheck::report::BaselineError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return 2;
  }
  std::cout << "baseline saved: " << all.size() << " violation(s) → " << file.string() << '\n';
  return 0;
}

// Filters `all` in-place. Returns suppressed count, or -1 on error (already printed to stderr).
int tryLoadAndFilter(archcheck::rules::ViolationList &all, const std::filesystem::path &file)
{
  try
  {
    const auto b = archcheck::report::loadBaseline(file);
    const auto filtered = archcheck::report::filterNew(all, b);
    const int suppressed = static_cast<int>(all.size() - filtered.size());
    all = filtered;
    return suppressed;
  }
  catch (const archcheck::report::BaselineError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return -1;
  }
}

// stdout colorization: on only for an interactive TTY with NO_COLOR unset
// (https://no-color.org). Piped/redirected output stays plain for CI logs.
bool textReportUseColor()
{
  const char *noColor = std::getenv("NO_COLOR");
  if (noColor != nullptr && noColor[0] != '\0')
    return false;
  return ::isatty(fileno(stdout)) != 0;
}

int applyBaselineAndReport(archcheck::rules::ViolationList all, OutputFormat fmt, const BaselineOpts &baseline)
{
  if (baseline.mode == BaselineMode::Save)
    return trySaveBaseline(all, baseline.file);

  std::size_t suppressed = 0;
  if (baseline.mode == BaselineMode::Load)
  {
    const int n = tryLoadAndFilter(all, baseline.file);
    if (n < 0)
      return 2;
    suppressed = static_cast<std::size_t>(n);
  }

  if (fmt == OutputFormat::Json)
    archcheck::report::writeJsonReport(all, std::cout);
  else
    archcheck::report::writeTextReport(all, std::cout, textReportUseColor());

  if (suppressed > 0 && fmt == OutputFormat::Text)
    std::cout << "suppressed: " << suppressed << " known violation(s) (run without --baseline to see all)\n";

  // Drift mode is a regression gate: only DRIFT.1 (new shortcut edge) and DRIFT.2
  // (new/grown cycle) gate the exit. Pre-existing intrinsic findings (SF.*/Lakos.*)
  // and the advisory DRIFT.3 module-coupling signal are reported but never fail a
  // drift run -- a legacy repo with no regression in this diff exits 0.
  if (baseline.driftFile)
  {
    const auto gating = std::count_if(all.begin(), all.end(),
                                      [](const auto &v) { return v.ruleId == "DRIFT.1" || v.ruleId == "DRIFT.2"; });
    if (fmt == OutputFormat::Text)
      std::cout << "drift gate: " << gating
                << " gating regression(s) (DRIFT.1/DRIFT.2); pre-existing and DRIFT.3 findings are advisory\n";
    return gating > 0 ? 1 : 0;
  }

  return all.empty() ? 0 : 1;
}

int applyDriftFile(const std::filesystem::path &driftFile, std::vector<std::unique_ptr<archcheck::rules::IRule>> &rules)
{
  std::ifstream in(driftFile);
  if (!in)
  {
    std::cerr << "archcheck: cannot open drift baseline: " << driftFile.string() << '\n';
    return 2;
  }
  auto [g, err] = archcheck::graph::loadBaseline(in);
  if (err)
  {
    std::cerr << "archcheck: drift baseline error: " << err->message << '\n';
    return 2;
  }
  for (auto &r : archcheck::rules::makeDriftRuleSet(std::move(g)))
    rules.push_back(std::move(r));
  return 0;
}

} // namespace

int runCheck(const std::filesystem::path &root, OutputFormat fmt, BaselineOpts baseline,
             std::optional<config::Config> config)
{
  if (!config)
  {
    try
    {
      config = archcheck::config::discover(root);
    }
    catch (const archcheck::config::ConfigError &e)
    {
      std::cerr << "archcheck: " << e.what() << '\n';
      return 2;
    }
  }
  const auto built = archcheck::graph::buildGraphForPath(root);
  archcheck::scan::DiskFileSource src(root);
  auto readFile = [&](std::string_view path) -> std::string { return src.read(std::string(path)); };
  auto rules = archcheck::rules::makeDefaultRuleSet(*config);
  const int driftRc = baseline.driftFile ? applyDriftFile(*baseline.driftFile, rules) : 0;
  if (driftRc != 0)
    return driftRc;
  archcheck::rules::ViolationList all;
  for (const auto &rule : rules)
  {
    auto v = rule->check(built.graph, readFile);
    all.insert(all.end(), v.begin(), v.end());
  }
  return applyBaselineAndReport(std::move(all), fmt, baseline);
}

int runSaveGraphBaseline(const std::filesystem::path &root, const std::filesystem::path &file)
{
  const auto built = archcheck::graph::buildGraphForPath(root);
  std::ofstream out(file);
  if (!out)
  {
    std::cerr << "archcheck: cannot write graph baseline: " << file.string() << '\n';
    return 2;
  }
  archcheck::graph::saveBaseline(built.graph, out);
  std::cout << "graph baseline saved: " << built.graph.nodeCount() << " node(s) \xe2\x86\x92 " << file.string() << '\n';
  return 0;
}

} // namespace archcheck::cli
