#include "cli/diff_command.h"

#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include "archcheck/config/config_loader.h"
#include "archcheck/diff/diff_json_report.h"
#include "archcheck/diff/md_report.h"
#include "archcheck/diff/regression_report.h"
#include "archcheck/git/diff_query.h"
#include "archcheck/git/git_exec.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/bool_field_drift.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/duplication/token_normalizer.h"
#include "archcheck/scan/flag_argument_scan.h"
#include "archcheck/scan/local_complexity_drift.h"
#include "archcheck/scan/local_complexity_metrics.h"
#include "archcheck/scan/new_clone_drift.h"
#include "archcheck/scan/satd_scan.h"
#include "archcheck/scan/source_snapshot.h"
#include "archcheck/scan/test_co_evolution.h"

#include "cli/check_command.h"

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

// Materialise one side of the revspec from a worktree (Disk mode): `git worktree add`
// via materializeOrReport, then build the graph from the on-disk tree. ok=false on a
// ref-materialisation failure. (Memory mode reuses the advisory snapshots instead — see
// buildDiffGraph — so it has no buildSide* path.)
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

// Bool-field accretion drift (#090/#135): a struct that existed in the baseline and gained
// net depth-0 bool fields — incremental boolean-state drift (constraint decay). Advisory
// only. vendored / test / generated files are dropped via the shared snapshot `authored`
// verdict (#129), so no separate exclusion list is reimplemented here.
archcheck::scan::BoolFieldDriftResult collectBoolFieldDrift(const std::filesystem::path &repoRoot,
                                                            const archcheck::git::Revspec &parsed,
                                                            const archcheck::scan::SourceSnapshot &baseSnap,
                                                            const archcheck::scan::SourceSnapshot &curSnap)
{
  const auto changed = archcheck::git::changedCppFiles(repoRoot, parsed.baseline, parsed.current);
  if (!changed || changed->empty())
    return {};
  return archcheck::scan::detectBoolFieldDrift(baseSnap, curSnap, *changed);
}

// New-clone drift (#123): copy-paste a commit introduces — duplication pairs in
// the new tree touching the diff's added lines. Advisory only. Consumes the shared
// per-ref snapshots (#129); the parent (baseline) snapshot feeds the parent-guard:
// clone pairs that already existed there are not introduced by this diff.
archcheck::scan::NewCloneDriftResult collectNewClones(const archcheck::scan::SourceSnapshot &curSnap,
                                                      const archcheck::scan::SourceSnapshot &baseSnap,
                                                      const std::vector<archcheck::git::AddedLine> &addedLines,
                                                      std::size_t maxCloneScanBytes)
{
  archcheck::scan::AddedLineMap added;
  for (const auto &a : addedLines)
    added[a.file].insert(a.lineNumber);
  if (added.empty())
    return {};
  return archcheck::scan::detectNewClones(curSnap, baseSnap, added, maxCloneScanBytes);
}

// Flag-argument drift (ARG.1, #093): boolean flag parameters in signatures the
// diff touches. Full-tree scan over authored sources, filtered to the commit's
// added lines (a signature flagged on an added line = introduced/changed here).
archcheck::rules::ViolationList collectFlagArguments(const archcheck::scan::SourceSnapshot &curSnap,
                                                     const std::vector<archcheck::git::AddedLine> &addedLines)
{
  std::unordered_set<std::string> addedKey;
  for (const auto &a : addedLines)
    addedKey.insert(a.file + ":" + std::to_string(a.lineNumber));
  if (addedKey.empty())
    return {};
  archcheck::rules::ViolationList out;
  for (const auto &[path, content] : curSnap.authoredSources())
  {
    const auto toks = archcheck::scan::stripDirectiveTokens(archcheck::scan::duplication::lex(content));
    for (auto &v : archcheck::scan::detectFlagArguments(toks, path))
      if (addedKey.count(v.file + ":" + std::to_string(v.line)) != 0)
        out.push_back(std::move(v));
  }
  return out;
}

// Advisory-only signals: SATD markers and test co-evolution over the changed
// lines, plus local complexity drift, new-clone drift and flag-argument drift.
// Reported after the structural diff, never gating.
struct DiffAdvisories
{
  archcheck::rules::ViolationList satd;
  archcheck::rules::ViolationList testCoEvolution;
  archcheck::scan::ComplexityDriftResult complexity;
  archcheck::scan::NewCloneDriftResult newClones;
  archcheck::rules::ViolationList flagArguments;
  archcheck::scan::BoolFieldDriftResult boolFields;
  std::size_t complexitySkippedAddedLines = 0; // >0: bulk import, advisory skipped (#117)
  // #129 read-once: the per-ref snapshots built here are kept so the graph build
  // reuses them in memory mode instead of re-reading both trees. Empty on a bulk
  // skip or an unresolvable ref (the graph then sees no snapshot ⇒ exit 2).
  std::optional<archcheck::scan::SourceSnapshot> baseSnapshot;
  std::optional<archcheck::scan::SourceSnapshot> currentSnapshot;
};

DiffAdvisories collectDiffAdvisories(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed,
                                     std::size_t maxAddedLines, std::size_t maxCloneScanBytes)
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
  // #129 read-once: one snapshot per ref, shared by the complexity/clone advisories
  // AND (kept on the result) the graph build, instead of each re-reading the trees.
  auto baseSnap = readRefSnapshotMemory(repoRoot, parsed.baseline);
  auto curSnap = readRefSnapshotMemory(repoRoot, parsed.current);
  if (!baseSnap || !curSnap)
    return result;
  result.complexity = collectComplexityDrift(repoRoot, parsed, *baseSnap, *curSnap);
  result.newClones = collectNewClones(*curSnap, *baseSnap, addedLines, maxCloneScanBytes);
  result.flagArguments = collectFlagArguments(*curSnap, addedLines);
  result.boolFields = collectBoolFieldDrift(repoRoot, parsed, *baseSnap, *curSnap);
  result.baseSnapshot = std::move(baseSnap);
  result.currentSnapshot = std::move(curSnap);
  return result;
}

// Print a "file:line: rule — message" advisory block under a header, or nothing
// when empty. Shared by the SATD / new-clone / flag-argument advisories.
void printViolationList(const char *header, const archcheck::rules::ViolationList &vs)
{
  if (vs.empty())
    return;
  std::cout << '\n' << header << ":\n";
  for (const auto &v : vs)
    std::cout << "  " << v.file << ":" << v.line << ": " << v.ruleId << " — " << v.message << '\n';
}

void printDiffAdvisories(const DiffAdvisories &a)
{
  printViolationList("self-admitted technical debt (advisory)", a.satd);
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
  if (a.newClones.skippedLargeTree)
    std::cout << "\nnew-clone drift (advisory): skipped — authored tree exceeds "
                 "thresholds.diff_max_clone_scan_bytes (#149)\n";
  printViolationList("new-clone drift (advisory)", a.newClones.violations);
  printViolationList("flag-argument drift (advisory)", a.flagArguments);
  printViolationList("bool-field accretion (advisory)", a.boolFields.violations);
}

// One flat list for the JSON document: SATD + test co-evolution + complexity.
archcheck::rules::ViolationList flattenAdvisories(DiffAdvisories a)
{
  archcheck::rules::ViolationList all = std::move(a.satd);
  all.insert(all.end(), a.testCoEvolution.begin(), a.testCoEvolution.end());
  all.insert(all.end(), a.complexity.violations.begin(), a.complexity.violations.end());
  all.insert(all.end(), a.newClones.violations.begin(), a.newClones.violations.end());
  all.insert(all.end(), a.flagArguments.begin(), a.flagArguments.end());
  all.insert(all.end(), a.boolFields.violations.begin(), a.boolFields.violations.end());
  return all;
}

// Config-derived knobs of the diff path: graph metric thresholds plus the
// bulk-import gate for advisories (#117).
struct DiffConfig
{
  archcheck::diff::MetricThresholds metric;
  std::size_t maxAddedLines = archcheck::config::Thresholds{}.diffMaxAddedLines;
  std::size_t maxCloneScanBytes = archcheck::config::Thresholds{}.diffMaxCloneScanBytes;
};

// Threshold discovery shared with check mode; nullopt = config error (exit 2).
std::optional<DiffConfig> loadDiffThresholds(const std::filesystem::path &repoRoot)
{
  DiffConfig cfg;
  try
  {
    const auto config = archcheck::config::discover(repoRoot);
    // Apply once here — before either side is read below — so baseline and current
    // are built on the same classification, no spurious drift from the override (#154 2b).
    applyClassificationConfig(config);
    cfg.metric.godHeaderFanIn = config.thresholds.godHeaderFanIn;
    cfg.maxAddedLines = config.thresholds.diffMaxAddedLines;
    cfg.maxCloneScanBytes = config.thresholds.diffMaxCloneScanBytes;
  }
  catch (const archcheck::config::ConfigError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return std::nullopt;
  }
  return cfg;
}

int emitJsonDiff(const archcheck::diff::RegressionReport &report, DiffAdvisories advisories,
                 const archcheck::git::Revspec &parsed, std::size_t renameSuppressed)
{
  const std::size_t skipped = advisories.complexitySkippedAddedLines;
  archcheck::diff::writeJsonReport(
      report, {parsed.baseline, parsed.current, flattenAdvisories(std::move(advisories)), skipped, renameSuppressed},
      std::cout);
  return report.gates() ? 1 : 0;
}

// True when the baseline ref resolves to a git object (the working-tree sentinel
// needs no resolution). On failure prints a diagnostic and returns false: an
// explicitly-given ref that does NOT resolve is a hard git error (exit 2), not a
// degradation — silently diffing against an empty tree turns a typo'd or un-fetched
// ref (common in shallow CI checkouts) into a phantom "everything added" gate on
// cycles that were always there. (#144, supersedes the #124 silent-empty warning.)
bool baselineResolves(const std::filesystem::path &repoRoot, const std::string &baselineRef)
{
  if (baselineRef == archcheck::git::kWorktreeRef)
    return true;
  const auto r = archcheck::git::runGit({"rev-parse", "--verify", "--quiet", baselineRef + "^{object}"}, repoRoot);
  if (r.exitCode == 0)
    return true;
  std::cerr << "archcheck: baseline ref '" << baselineRef
            << "' does not resolve to a git object; cannot diff. Fetch the ref (shallow CI: fetch "
               "the base with --depth=1) or check the revspec.\n";
  return false;
}

// Both graph sides + the regression report (memory or disk backend). ok=false on a
// ref-materialisation failure (caller returns exit 2). Only built for non-bulk diffs.
struct DiffGraph
{
  archcheck::diff::RegressionReport report;
  std::size_t baselineNodes = 0;
  std::size_t currentNodes = 0;
  bool ok = true;
  std::size_t renameSuppressed = 0; // grown cycles dropped as mass-rename artifacts (#133)
};

// Memory mode reuses the advisory snapshots (#129 read-once: both trees were already
// read+classified in collectDiffAdvisories), so the refs are not read a second time. A
// null snapshot means an unresolvable ref (advisories returned early) ⇒ ok=false ⇒ exit 2.
// Disk mode builds from worktrees — a different backend that cannot share memory snapshots.
DiffGraph buildDiffGraph(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed, DiffMode mode,
                         const archcheck::diff::MetricThresholds &metric, const DiffAdvisories &advisories)
{
  archcheck::graph::GraphBuildResult baseline, current;
  if (mode == DiffMode::Memory)
  {
    if (!advisories.baseSnapshot || !advisories.currentSnapshot)
      return {{}, 0, 0, false};
    baseline = archcheck::graph::buildGraphForSnapshot(*advisories.baseSnapshot);
    current = archcheck::graph::buildGraphForSnapshot(*advisories.currentSnapshot);
  }
  else
  {
    bool okBase = false, okCurr = false;
    baseline = buildSideDisk(repoRoot, parsed.baseline, "baseline", okBase);
    current = buildSideDisk(repoRoot, parsed.current, "current", okCurr);
    if (!okBase || !okCurr)
      return {{}, 0, 0, false};
  }
  return {archcheck::diff::buildRegressionReport(baseline.graph, current.graph, metric), baseline.graph.nodeCount(),
          current.graph.nodeCount(), true};
}

// A mass include move re-paths pre-existing cycles into phantom "new" SCCs; drop
// the ones whose every member was renamed in this diff (#133).
void applyRenameArtifactGuard(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed,
                              DiffGraph &graph)
{
  if (graph.report.grownCycles.empty())
    return;
  const auto renamed = archcheck::git::collectRenamedPaths(repoRoot, parsed.baseline, parsed.current);
  graph.renameSuppressed = archcheck::diff::dropRenameArtifactCycles(graph.report, {renamed.begin(), renamed.end()});
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
  if (graph.renameSuppressed > 0)
    std::cout << "\nnote: " << graph.renameSuppressed
              << " grown cycle(s) suppressed as rename artifacts of a mass include move (#133)\n";
  printDiffAdvisories(advisories);
}

int runDiffFullPath(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed, DiffMode mode,
                    OutputFormat format)
{
  const auto thresholds = loadDiffThresholds(repoRoot);
  if (!thresholds)
    return 2;
  if (!baselineResolves(repoRoot, parsed.baseline))
    return 2;

  // Advisories first: they compute the bulk-import signal (#117). A bulk import is not
  // authored evolution (vendored / generated / "committed as-is, fix later"), so gating a
  // merge over its graph is unfair and noisy — skip the graph checks (gating AND drift) on
  // bulk commits, as clone/complexity already do; archcheck's job is slow drift. (#124)
  auto advisories = collectDiffAdvisories(repoRoot, parsed, thresholds->maxAddedLines, thresholds->maxCloneScanBytes);
  const bool bulk = advisories.complexitySkippedAddedLines > 0;

  DiffGraph graph; // bulk ⇒ stays empty (no gating, no drift)
  if (!bulk)
  {
    graph = buildDiffGraph(repoRoot, parsed, mode, thresholds->metric, advisories);
    if (!graph.ok)
      return 2;
    applyRenameArtifactGuard(repoRoot, parsed, graph);
  }

  if (format == OutputFormat::Json)
    return emitJsonDiff(graph.report, std::move(advisories), parsed, graph.renameSuppressed);
  if (format == OutputFormat::Markdown)
  {
    archcheck::diff::writeMdReport(graph.report, std::cout);
    return graph.report.gates() ? 1 : 0;
  }
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
