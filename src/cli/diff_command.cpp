#include "cli/diff_command.h"

#include <iostream>
#include <optional>
#include <string>

#include "archcheck/config/config_loader.h"
#include "archcheck/diff/regression_report.h"
#include "archcheck/git/diff_query.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/local_complexity_drift.h"
#include "archcheck/scan/satd_scan.h"
#include "archcheck/scan/test_co_evolution.h"

namespace archcheck::cli
{

namespace
{

std::optional<archcheck::git::Worktree> materializeOrReport(const std::filesystem::path &repoRoot,
                                                            const std::string &ref, const char *role)
{
  archcheck::git::GitError err;
  auto tree = archcheck::git::materializeRef(repoRoot, ref, err);
  if (!tree)
  {
    std::cerr << "archcheck: cannot materialize " << role << " ref '" << ref << "': " << err.message << '\n';
  }
  return tree;
}

// Returns true and prints a one-shot report if the fast-path applies
// (zero C/C++ files changed between baseline and current → graph cannot have
// changed). Caller in that case skips the worktree materialisation.
bool tryFastPathNoCppChanges(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed)
{
  const auto changed = archcheck::git::changedCppFiles(repoRoot, parsed.baseline, parsed.current);
  if (!changed || !changed->empty())
    return false;
  std::cout << "baseline_ref:   " << parsed.baseline << '\n'
            << "current_ref:    " << parsed.current << '\n'
            << "no C/C++ files changed; skipping graph build\n";
  return true;
}

// Materialise one side of the revspec as a FileSource. For WORKTREE always
// use the on-disk source. For real refs: in Memory mode read blobs directly;
// in Disk mode go through `git worktree add` via materializeRef.
archcheck::graph::GraphBuildResult buildSideMemory(const std::filesystem::path &repoRoot, const std::string &ref,
                                                   bool &ok)
{
  if (ref == archcheck::git::kWorktreeRef)
  {
    archcheck::scan::DiskFileSource src(repoRoot);
    ok = true;
    return archcheck::graph::buildGraphForSource(src);
  }
  archcheck::git::GitObjectFileSource src(repoRoot, ref);
  if (!src.valid())
  {
    ok = false;
    return {};
  }
  ok = true;
  return archcheck::graph::buildGraphForSource(src);
}

archcheck::graph::GraphBuildResult buildSideDisk(const std::filesystem::path &repoRoot, const std::string &ref,
                                                 const char *role, bool &ok)
{
  auto tree = materializeOrReport(repoRoot, ref, role);
  if (!tree)
  {
    ok = false;
    return {};
  }
  ok = true;
  return archcheck::graph::buildGraphForPath(tree->path());
}

void printComplexityResult(const archcheck::scan::ComplexityDriftResult &drift)
{
  if (drift.violations.empty() && drift.negativeDelta == 0)
    return;
  std::cout << "\nlocal complexity drift (advisory):\n";
  for (const auto &v : drift.violations)
    std::cout << "  " << v.file << ":" << v.line << ": " << v.ruleId << " — " << v.message << '\n';
  std::cout << "  net complexity delta: +" << drift.positiveDelta;
  if (drift.negativeDelta < 0)
    std::cout << " (improvements: " << drift.negativeDelta << ")";
  std::cout << '\n';
}

// Local complexity drift (#101): per-function cognitive complexity compared
// between baseline and current versions of the changed C/C++ files.
void printComplexityAdvisory(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed)
{
  const auto changed = archcheck::git::changedCppFiles(repoRoot, parsed.baseline, parsed.current);
  if (!changed || changed->empty())
    return;
  archcheck::git::GitObjectFileSource oldSource(repoRoot, parsed.baseline);
  if (!oldSource.valid())
    return;
  if (parsed.current == archcheck::git::kWorktreeRef)
  {
    archcheck::scan::DiskFileSource newSource(repoRoot);
    printComplexityResult(archcheck::scan::detectLocalComplexityDrift(oldSource, newSource, *changed));
    return;
  }
  archcheck::git::GitObjectFileSource newSource(repoRoot, parsed.current);
  if (newSource.valid())
    printComplexityResult(archcheck::scan::detectLocalComplexityDrift(oldSource, newSource, *changed));
}

// Advisory-only signals over the changed lines (SATD markers, test
// co-evolution): reported after the structural diff, never gating.
void printDiffAdvisories(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed)
{
  const auto addedLines = archcheck::git::collectAddedLines(repoRoot, parsed.baseline, parsed.current);
  const auto satdViolations = archcheck::scan::detectSatdMarkers(addedLines);
  if (!satdViolations.empty())
  {
    std::cout << "\nself-admitted technical debt (advisory):\n";
    for (const auto &v : satdViolations)
      std::cout << "  " << v.file << ":" << v.line << ": " << v.ruleId << " — " << v.message << '\n';
  }

  const auto numstatEntries = archcheck::git::collectNumstat(repoRoot, parsed.baseline, parsed.current);
  const auto testCoEvolViolations = archcheck::scan::detectTestCoEvolution(numstatEntries);
  if (!testCoEvolViolations.empty())
  {
    std::cout << "\ntest co-evolution (advisory):\n";
    for (const auto &v : testCoEvolViolations)
      std::cout << "  " << v.ruleId << ": " << v.message << '\n';
  }

  printComplexityAdvisory(repoRoot, parsed);
}

int runDiffFullPath(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed, DiffMode mode)
{
  archcheck::diff::MetricThresholds thresholds;
  try
  {
    thresholds.godHeaderFanIn = archcheck::config::discover(repoRoot).thresholds.godHeaderFanIn;
  }
  catch (const archcheck::config::ConfigError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return 2;
  }
  bool okBase = false, okCurr = false;
  const auto baseline = (mode == DiffMode::Memory) ? buildSideMemory(repoRoot, parsed.baseline, okBase)
                                                   : buildSideDisk(repoRoot, parsed.baseline, "baseline", okBase);
  const auto current = (mode == DiffMode::Memory) ? buildSideMemory(repoRoot, parsed.current, okCurr)
                                                  : buildSideDisk(repoRoot, parsed.current, "current", okCurr);
  if (!okBase || !okCurr)
    return 2;

  const auto report = archcheck::diff::buildRegressionReport(baseline.graph, current.graph, thresholds);
  std::cout << "baseline_ref:   " << parsed.baseline << '\n'
            << "current_ref:    " << parsed.current << '\n'
            << "diff_mode:      " << (mode == DiffMode::Memory ? "memory" : "disk") << '\n'
            << "baseline_nodes: " << baseline.graph.nodeCount() << '\n'
            << "current_nodes:  " << current.graph.nodeCount() << '\n';
  archcheck::diff::writeTextReport(report, std::cout);
  printDiffAdvisories(repoRoot, parsed);
  return report.hasRegression() ? 1 : 0;
}

} // namespace

int runDiff(std::string_view revspec, const std::filesystem::path &root, DiffMode mode)
{
  const auto parsed = archcheck::git::parseRevspec(revspec);
  if (!parsed)
  {
    std::cerr << "archcheck: invalid revspec '" << revspec << "' (expected 'a..b' or '<ref>')\n";
    return 2;
  }
  const auto repoRoot = archcheck::git::findRepoRoot(root);
  if (!repoRoot)
  {
    std::cerr << "archcheck: '" << root.string() << "' is not inside a git repository\n";
    return 2;
  }
  if (tryFastPathNoCppChanges(*repoRoot, *parsed))
    return 0;
  return runDiffFullPath(*repoRoot, *parsed, mode);
}

} // namespace archcheck::cli
