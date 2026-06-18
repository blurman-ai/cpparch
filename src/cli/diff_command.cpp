#include "cli/diff_command.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "archcheck/config/config_loader.h"
#include "archcheck/diff/diff_json_report.h"
#include "archcheck/diff/regression_report.h"
#include "archcheck/git/diff_query.h"
#include "archcheck/git/git_exec.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/local_complexity_drift.h"
#include "archcheck/scan/new_clone_drift.h"
#include "archcheck/scan/satd_scan.h"
#include "archcheck/scan/source_snapshot.h"
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
bool tryFastPathNoCppChanges(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed,
                             archcheck::cli::OutputFormat format)
{
  const auto changed = archcheck::git::changedCppFiles(repoRoot, parsed.baseline, parsed.current);
  if (!changed || !changed->empty())
    return false;
  if (format == archcheck::cli::OutputFormat::Json)
  {
    archcheck::diff::writeJsonReport({}, {parsed.baseline, parsed.current, {}}, std::cout);
    return true;
  }
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

// Read+classify one ref's tree into a snapshot for the memory advisories
// (complexity + clone). WORKTREE => the on-disk source; otherwise blobs via git
// cat-file. nullopt if the ref does not resolve. One snapshot per ref is shared
// by both advisories so each tree is read once (#129 read-once).
std::optional<archcheck::scan::SourceSnapshot> readRefSnapshotMemory(const std::filesystem::path &repoRoot,
                                                                     const std::string &ref)
{
  if (ref == archcheck::git::kWorktreeRef)
  {
    archcheck::scan::DiskFileSource src(repoRoot);
    return archcheck::scan::SourceSnapshot::read(src);
  }
  archcheck::git::GitObjectFileSource src(repoRoot, ref);
  if (!src.valid())
    return std::nullopt;
  return archcheck::scan::SourceSnapshot::read(src);
}

// Local complexity drift (#101): per-function cognitive complexity compared
// between the baseline and current versions of the changed C/C++ files. Consumes
// the shared per-ref snapshots (#129).
archcheck::scan::ComplexityDriftResult collectComplexityDrift(const std::filesystem::path &repoRoot,
                                                              const archcheck::git::Revspec &parsed,
                                                              const archcheck::scan::SourceSnapshot &baseSnap,
                                                              const archcheck::scan::SourceSnapshot &curSnap)
{
  const auto changed = archcheck::git::changedCppFiles(repoRoot, parsed.baseline, parsed.current);
  if (!changed || changed->empty())
    return {};
  return archcheck::scan::detectLocalComplexityDrift(baseSnap, curSnap, *changed);
}

// New-clone drift (#123): copy-paste a commit introduces — duplication pairs in
// the new tree touching the diff's added lines. Advisory only. Consumes the shared
// per-ref snapshots (#129); the parent (baseline) snapshot feeds the parent-guard:
// clone pairs that already existed there are not introduced by this diff.
archcheck::scan::NewCloneDriftResult collectNewClones(const archcheck::scan::SourceSnapshot &curSnap,
                                                      const archcheck::scan::SourceSnapshot &baseSnap,
                                                      const std::vector<archcheck::git::AddedLine> &addedLines)
{
  archcheck::scan::AddedLineMap added;
  for (const auto &a : addedLines)
    added[a.file].insert(a.lineNumber);
  if (added.empty())
    return {};
  return archcheck::scan::detectNewClones(curSnap, baseSnap, added);
}

// Advisory-only signals: SATD markers and test co-evolution over the changed
// lines, plus local complexity drift and new-clone drift. Reported after the
// structural diff, never gating.
struct DiffAdvisories
{
  archcheck::rules::ViolationList satd;
  archcheck::rules::ViolationList testCoEvolution;
  archcheck::scan::ComplexityDriftResult complexity;
  archcheck::scan::NewCloneDriftResult newClones;
  std::size_t complexitySkippedAddedLines = 0; // >0: bulk import, advisory skipped (#117)
};

DiffAdvisories collectDiffAdvisories(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed,
                                     std::size_t maxAddedLines)
{
  DiffAdvisories result;
  const auto addedLines = archcheck::git::collectAddedLines(repoRoot, parsed.baseline, parsed.current);
  result.satd = archcheck::scan::detectSatdMarkers(addedLines);
  const auto numstatEntries = archcheck::git::collectNumstat(repoRoot, parsed.baseline, parsed.current);
  result.testCoEvolution = archcheck::scan::detectTestCoEvolution(numstatEntries);
  // Bulk source imports are not authored evolution — their per-function
  // complexity findings are mechanically true but pure volume noise (#117).
  const std::size_t totalAdded = archcheck::git::totalAddedLines(numstatEntries);
  if (totalAdded > maxAddedLines)
  {
    result.complexitySkippedAddedLines = totalAdded;
    return result;
  }
  // #129 read-once: one snapshot per ref, read+classified here and shared by both
  // the complexity and the clone advisory (each used to read the trees itself).
  const auto baseSnap = readRefSnapshotMemory(repoRoot, parsed.baseline);
  const auto curSnap = readRefSnapshotMemory(repoRoot, parsed.current);
  if (!baseSnap || !curSnap)
    return result;
  result.complexity = collectComplexityDrift(repoRoot, parsed, *baseSnap, *curSnap);
  result.newClones = collectNewClones(*curSnap, *baseSnap, addedLines);
  return result;
}

void printDiffAdvisories(const DiffAdvisories &a)
{
  if (!a.satd.empty())
  {
    std::cout << "\nself-admitted technical debt (advisory):\n";
    for (const auto &v : a.satd)
      std::cout << "  " << v.file << ":" << v.line << ": " << v.ruleId << " — " << v.message << '\n';
  }
  if (!a.testCoEvolution.empty())
  {
    std::cout << "\ntest co-evolution (advisory):\n";
    for (const auto &v : a.testCoEvolution)
      std::cout << "  " << v.ruleId << ": " << v.message << '\n';
  }
  if (a.complexitySkippedAddedLines > 0)
  {
    std::cout << "\nlocal complexity drift (advisory): skipped — diff adds " << a.complexitySkippedAddedLines
              << " lines (bulk import; thresholds.diff_max_added_lines)\n";
    return;
  }
  printComplexityResult(a.complexity);
  if (!a.newClones.violations.empty())
  {
    std::cout << "\nnew-clone drift (advisory):\n";
    for (const auto &v : a.newClones.violations)
      std::cout << "  " << v.file << ":" << v.line << ": " << v.ruleId << " — " << v.message << '\n';
  }
}

// One flat list for the JSON document: SATD + test co-evolution + complexity.
archcheck::rules::ViolationList flattenAdvisories(DiffAdvisories a)
{
  archcheck::rules::ViolationList all = std::move(a.satd);
  all.insert(all.end(), a.testCoEvolution.begin(), a.testCoEvolution.end());
  all.insert(all.end(), a.complexity.violations.begin(), a.complexity.violations.end());
  all.insert(all.end(), a.newClones.violations.begin(), a.newClones.violations.end());
  return all;
}

// Config-derived knobs of the diff path: graph metric thresholds plus the
// bulk-import gate for advisories (#117).
struct DiffConfig
{
  archcheck::diff::MetricThresholds metric;
  std::size_t maxAddedLines = archcheck::config::Thresholds{}.diffMaxAddedLines;
};

// Threshold discovery shared with check mode; nullopt = config error (exit 2).
std::optional<DiffConfig> loadDiffThresholds(const std::filesystem::path &repoRoot)
{
  DiffConfig cfg;
  try
  {
    const auto thresholds = archcheck::config::discover(repoRoot).thresholds;
    cfg.metric.godHeaderFanIn = thresholds.godHeaderFanIn;
    cfg.maxAddedLines = thresholds.diffMaxAddedLines;
  }
  catch (const archcheck::config::ConfigError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return std::nullopt;
  }
  return cfg;
}

int emitJsonDiff(const archcheck::diff::RegressionReport &report, DiffAdvisories advisories,
                 const archcheck::git::Revspec &parsed)
{
  const std::size_t skipped = advisories.complexitySkippedAddedLines;
  archcheck::diff::writeJsonReport(
      report, {parsed.baseline, parsed.current, flattenAdvisories(std::move(advisories)), skipped}, std::cout);
  return report.gates() ? 1 : 0;
}

// Warn (do not fail) when the baseline ref does not resolve. The diff then degrades
// to "whole current revision vs empty tree", which is correct for a real initial
// commit but silently hides a typo'd ref. Surface it on stderr without touching the
// exit code or gating. (#124 DEBT: silent empty-baseline.)
void warnIfBaselineUnresolved(const std::filesystem::path &repoRoot, const std::string &baselineRef)
{
  if (baselineRef == archcheck::git::kWorktreeRef)
    return;
  const auto r = archcheck::git::runGit({"rev-parse", "--verify", "--quiet", baselineRef + "^{object}"}, repoRoot);
  if (r.exitCode != 0)
    std::cerr << "archcheck: warning: baseline ref '" << baselineRef
              << "' does not resolve; diffing against an empty tree (the whole current revision "
                 "is treated as added). Check the revspec if this was unintended.\n";
}

// Both graph sides + the regression report (memory or disk backend). ok=false on a
// ref-materialisation failure (caller returns exit 2). Only built for non-bulk diffs.
struct DiffGraph
{
  archcheck::diff::RegressionReport report;
  std::size_t baselineNodes = 0;
  std::size_t currentNodes = 0;
  bool ok = true;
};

DiffGraph buildDiffGraph(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed, DiffMode mode,
                         const archcheck::diff::MetricThresholds &metric)
{
  bool okBase = false, okCurr = false;
  const auto baseline = (mode == DiffMode::Memory) ? buildSideMemory(repoRoot, parsed.baseline, okBase)
                                                   : buildSideDisk(repoRoot, parsed.baseline, "baseline", okBase);
  const auto current = (mode == DiffMode::Memory) ? buildSideMemory(repoRoot, parsed.current, okCurr)
                                                  : buildSideDisk(repoRoot, parsed.current, "current", okCurr);
  if (!okBase || !okCurr)
    return {{}, 0, 0, false};
  return {archcheck::diff::buildRegressionReport(baseline.graph, current.graph, metric), baseline.graph.nodeCount(),
          current.graph.nodeCount(), true};
}

void printDiffText(const archcheck::git::Revspec &parsed, DiffMode mode, bool bulk, const DiffGraph &graph,
                   const DiffAdvisories &advisories)
{
  std::cout << "baseline_ref:   " << parsed.baseline << '\n'
            << "current_ref:    " << parsed.current << '\n'
            << "diff_mode:      " << (mode == DiffMode::Memory ? "memory" : "disk") << '\n';
  if (bulk)
    std::cout << "graph checks:   skipped — diff adds " << advisories.complexitySkippedAddedLines
              << " lines (bulk import; thresholds.diff_max_added_lines)\n";
  else
    std::cout << "baseline_nodes: " << graph.baselineNodes << '\n' << "current_nodes:  " << graph.currentNodes << '\n';
  archcheck::diff::writeTextReport(graph.report, std::cout);
  printDiffAdvisories(advisories);
}

int runDiffFullPath(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed, DiffMode mode,
                    OutputFormat format)
{
  const auto thresholds = loadDiffThresholds(repoRoot);
  if (!thresholds)
    return 2;
  warnIfBaselineUnresolved(repoRoot, parsed.baseline);

  // Advisories first: they compute the bulk-import signal (#117). A bulk import is
  // not the project's authored evolution (vendored / generated / "committed as-is,
  // fix later" / not even the author's code), so block-gating a merge over its graph
  // is unfair and noisy. Skip the graph checks (gating AND drift) on bulk commits,
  // as clone/complexity already do — archcheck's job is slow incremental drift. (#124)
  auto advisories = collectDiffAdvisories(repoRoot, parsed, thresholds->maxAddedLines);
  const bool bulk = advisories.complexitySkippedAddedLines > 0;

  DiffGraph graph; // bulk ⇒ stays empty (no gating, no drift)
  if (!bulk)
  {
    graph = buildDiffGraph(repoRoot, parsed, mode, thresholds->metric);
    if (!graph.ok)
      return 2;
  }

  if (format == OutputFormat::Json)
    return emitJsonDiff(graph.report, std::move(advisories), parsed);
  printDiffText(parsed, mode, bulk, graph, advisories);
  return graph.report.gates() ? 1 : 0;
}

} // namespace

int runDiff(std::string_view revspec, const std::filesystem::path &root, DiffMode mode, OutputFormat format)
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
  if (tryFastPathNoCppChanges(*repoRoot, *parsed, format))
    return 0;
  return runDiffFullPath(*repoRoot, *parsed, mode, format);
}

} // namespace archcheck::cli
