#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "archcheck/config/config_loader.h"
#include "archcheck/diff/regression_report.h"
#include "archcheck/git/diff_query.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/git/history_query.h"
#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/baseline.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/report/json_reporter.h"
#include "archcheck/report/text_reporter.h"
#include "archcheck/report/violation_baseline.h"
#include "archcheck/rules/rule_set.h"
#include "archcheck/scan/defect_attractor.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/duplication/duplication_scanner.h"
#include "archcheck/scan/god_file_growth.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"
#include "archcheck/scan/satd_scan.h"
#include "archcheck/scan/test_co_evolution.h"
#include "archcheck/version.h"

namespace
{

void print_version() { std::cout << "archcheck " << archcheck::kVersionString << '\n'; }

void print_help()
{
  std::cout
      << "archcheck - architecture rules for C++ projects\n"
      << "\n"
      << "Usage:\n"
      << "  archcheck [path]                             (check: run all default rules on path or cwd)\n"
      << "  archcheck --format json [path]               (JSON output)\n"
      << "  archcheck --config <path> [check-path]       (validate .archcheck.yml v1 + apply thresholds; module "
         "rules not yet enforced)\n"
      << "  archcheck --save-baseline <file> [path]      (save current violations as baseline)\n"
      << "  archcheck --baseline <file> [path]           (report only new violations vs baseline)\n"
      << "  archcheck --save-graph-baseline <file> [path] (save include graph snapshot for drift checks)\n"
      << "  archcheck --drift-baseline <file> [path]    (drift gate: DRIFT.1/DRIFT.2 fail the run; "
         "DRIFT.3 + pre-existing findings advisory)\n"
      << "  archcheck --version\n"
      << "  archcheck --help\n"
      << "  archcheck --scan  <path>                     (preview: discover + scan #includes)\n"
      << "  archcheck --graph <path>                     (preview: build dependency graph + SCC stats)\n"
      << "  archcheck --duplication <path>               (report duplicate code; advisory, does not gate CI)\n"
      << "  archcheck --history <path>                   (history analytics: god-file growth; advisory, does not "
         "gate CI)\n"
      << "  archcheck --diff  [--diff-mode=disk|memory] <revspec> [path]\n"
      << "                                               (regression vs git ref; revspec = 'a..b' or '<ref>')\n"
      << "\n"
      << "Default rules (no config required): SF.7, SF.8, SF.9, Lakos.GodHeader, Lakos.ChainLength\n"
      << "Default thresholds: chain_length=10, god_header_fan_in=50 (override via thresholds: in .archcheck.yml)\n"
      << "Drift rules (require --drift-baseline):        DRIFT.1 (shortcut edges), DRIFT.2 (cycle growth) "
         "[gating]; DRIFT.3 (module coupling) [advisory]\n";
}

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
    archcheck::report::writeTextReport(all, std::cout);

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

// Walkup-discovers and loads .archcheck.yml from the CWD; embedded defaults if none.
archcheck::config::Config discoverConfig()
{
  const auto found = archcheck::config::findConfig(std::filesystem::current_path());
  return found ? archcheck::config::load(*found) : archcheck::config::Config{};
}

int run_check(const std::filesystem::path &root, OutputFormat fmt, BaselineOpts baseline = {},
              std::optional<archcheck::config::Config> config = std::nullopt)
{
  if (!config)
  {
    try
    {
      config = discoverConfig();
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

int run_save_graph_baseline(const std::filesystem::path &root, const std::filesystem::path &file)
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

// S4: upper bound on individual source file size to prevent OOM.
static constexpr std::size_t kMainMaxFileSizeBytes = 64ULL * 1024 * 1024; // 64 MiB

std::string read_file(const std::filesystem::path &p)
{
  std::error_code ec;
  const auto sz = std::filesystem::file_size(p, ec);
  if (!ec && sz > kMainMaxFileSizeBytes)
  {
    std::cerr << "archcheck: skipping oversized file (> 64 MiB): " << p.string() << '\n';
    return {};
  }
  std::ifstream f(p, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

int run_scan(const std::filesystem::path &root)
{
  const auto files = archcheck::scan::discoverFiles(root);
  std::size_t directives = 0;
  std::size_t quotes = 0;
  std::size_t angles = 0;
  std::size_t diagnostics = 0;
  for (const auto &f : files)
  {
    const auto res = archcheck::scan::scanIncludes(read_file(root / f.path));
    directives += res.directives.size();
    diagnostics += res.diagnostics.size();
    for (const auto &d : res.directives)
    {
      (d.kind == archcheck::scan::IncludeKind::Quote ? quotes : angles)++;
    }
  }
  std::cout << "files:       " << files.size() << '\n'
            << "directives:  " << directives << " (quote=" << quotes << ", angle=" << angles << ")\n"
            << "diagnostics: " << diagnostics << '\n';
  return 0;
}

struct SccStats
{
  std::size_t total = 0;
  std::size_t cyclic = 0;
  std::size_t largest = 0;
};

SccStats compute_scc_stats(const archcheck::graph::DependencyGraph &dg)
{
  SccStats s;
  const auto sccs = archcheck::graph::computeScc(dg);
  s.total = sccs.size();
  for (const auto &c : sccs)
  {
    if (c.size() >= 2)
      ++s.cyclic;
    if (c.size() > s.largest)
      s.largest = c.size();
  }
  return s;
}

int run_graph(const std::filesystem::path &root)
{
  const auto built = archcheck::graph::buildGraphForPath(root);
  const auto &c = built.counters;
  const auto scc = compute_scc_stats(built.graph);
  std::cout << "nodes:          " << built.graph.nodeCount() << '\n'
            << "edges:          " << c.edges << '\n'
            << "external:       " << c.external << '\n'
            << "unresolved:     " << c.unresolved << '\n'
            << "ambiguous:      " << c.ambiguous << '\n'
            << "macro_includes: " << c.macro_includes << '\n'
            << "sccs_total:     " << scc.total << '\n'
            << "sccs_cyclic:    " << scc.cyclic << '\n'
            << "largest_scc:    " << scc.largest << '\n';
  return scc.cyclic == 0 ? 0 : 1;
}

int run_duplication(const std::filesystem::path &root)
{
  // Exclude vendored code (noise in every signal), mirroring the graph builder.
  archcheck::scan::DiskFileSource diskSrc(root);
  const auto sources = archcheck::scan::collectNonVendoredSources(diskSrc);
  const auto result = archcheck::scan::duplication::scanForDuplication(sources);

  // Re-sort by weighted desc: P1 classifiers down-weight pairs after the scanner's sort.
  auto pairs = result.pairs;
  std::sort(pairs.begin(), pairs.end(), [](const auto &l, const auto &r) { return l.weighted > r.weighted; });

  std::cout << "scanned " << result.fileCount << " files, " << result.fragments.size() << " fragments, "
            << result.candidateCount << " candidate pairs\n"
            << "reported " << pairs.size() << " pairs above threshold (" << result.wholeFileClones
            << " whole-file clone group(s) suppressed)\n";
  for (const auto &p : pairs)
  {
    const auto &fa = result.fragments[p.a];
    const auto &fb = result.fragments[p.b];
    std::cout << "  " << fa.file << ":" << fa.startLine << "-" << fa.endLine << "  <->  " << fb.file << ":"
              << fb.startLine << "-" << fb.endLine << "  (" << p.type << ", weighted=" << p.weighted
              << ", line=" << p.line << ")\n";
  }

  return 0;
}

static std::map<std::string, std::int32_t> buildLocMap(const std::filesystem::path &root)
{
  archcheck::scan::DiskFileSource diskSrc(root);
  const auto sources = archcheck::scan::collectNonVendoredSources(diskSrc);
  std::map<std::string, std::int32_t> locMap;
  for (const auto &[path, content] : sources)
  {
    locMap[path] = static_cast<std::int32_t>(std::count(content.begin(), content.end(), '\n'));
  }
  return locMap;
}

int run_history(const std::filesystem::path &root)
{
  const auto locMap = buildLocMap(root);
  const auto history = archcheck::git::queryCommitHistory(root, 200);

  const archcheck::scan::GodFileGrowthDetector detector(locMap, history);
  const auto godFileViolations = detector.detect();
  std::cout << "queried " << history.size() << " commits, found " << godFileViolations.size()
            << " god-file growth candidate(s)\n";
  for (const auto &v : godFileViolations)
  {
    std::cout << v.file << ": [" << v.ruleId << "] " << v.message << '\n';
  }

  const archcheck::scan::DefectAttractorDetector defectDetector(history);
  const auto defectViolations = defectDetector.detect();
  if (!defectViolations.empty())
  {
    std::cout << "\ndefect attractors (advisory): " << defectViolations.size() << " file(s)\n";
    for (const auto &v : defectViolations)
    {
      std::cout << v.file << ": [" << v.ruleId << "] " << v.message << '\n';
    }
  }
  return 0;
}

std::optional<archcheck::git::Worktree> materialize_or_report(const std::filesystem::path &repoRoot,
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

enum class DiffMode
{
  Memory, // git cat-file --batch (default)
  Disk    // git worktree add (legacy fallback)
};

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
  auto tree = materialize_or_report(repoRoot, ref, role);
  if (!tree)
  {
    ok = false;
    return {};
  }
  ok = true;
  return archcheck::graph::buildGraphForPath(tree->path());
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
}

int runDiffFullPath(const std::filesystem::path &repoRoot, const archcheck::git::Revspec &parsed, DiffMode mode)
{
  bool okBase = false, okCurr = false;
  const auto baseline = (mode == DiffMode::Memory) ? buildSideMemory(repoRoot, parsed.baseline, okBase)
                                                   : buildSideDisk(repoRoot, parsed.baseline, "baseline", okBase);
  const auto current = (mode == DiffMode::Memory) ? buildSideMemory(repoRoot, parsed.current, okCurr)
                                                  : buildSideDisk(repoRoot, parsed.current, "current", okCurr);
  if (!okBase || !okCurr)
    return 2;

  const auto report = archcheck::diff::buildRegressionReport(baseline.graph, current.graph);
  std::cout << "baseline_ref:   " << parsed.baseline << '\n'
            << "current_ref:    " << parsed.current << '\n'
            << "diff_mode:      " << (mode == DiffMode::Memory ? "memory" : "disk") << '\n'
            << "baseline_nodes: " << baseline.graph.nodeCount() << '\n'
            << "current_nodes:  " << current.graph.nodeCount() << '\n';
  archcheck::diff::writeTextReport(report, std::cout);
  printDiffAdvisories(repoRoot, parsed);
  return report.hasRegression() ? 1 : 0;
}

int run_diff(std::string_view revspec, const std::filesystem::path &root, DiffMode mode)
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

bool parseDiffMode(std::string_view raw, DiffMode &out)
{
  if (raw == "memory")
  {
    out = DiffMode::Memory;
    return true;
  }
  if (raw == "disk")
  {
    out = DiffMode::Disk;
    return true;
  }
  return false;
}

// Strip a leading `--diff-mode=...` from argv starting at `idx`. Returns
// the new idx on success; -1 on malformed value. Mutates `mode` in place.
int consumeDiffModeFlag(int argc, char *argv[], int idx, DiffMode &mode)
{
  constexpr std::string_view kPrefix = "--diff-mode=";
  while (idx < argc)
  {
    const std::string_view a{argv[idx]};
    if (a.size() <= kPrefix.size() || a.compare(0, kPrefix.size(), kPrefix) != 0)
      return idx;
    if (!parseDiffMode(a.substr(kPrefix.size()), mode))
    {
      std::cerr << "archcheck: invalid --diff-mode value '" << a.substr(kPrefix.size())
                << "' (expected 'disk' or 'memory')\n";
      return -1;
    }
    ++idx;
  }
  return idx;
}

int dispatch_diff(int argc, char *argv[])
{
  DiffMode mode = DiffMode::Memory;
  const int idx = consumeDiffModeFlag(argc, argv, 2, mode);
  if (idx < 0)
    return 2;
  if (idx >= argc)
  {
    std::cerr << "archcheck: --diff requires <revspec> [path]\n";
    return 2;
  }
  const std::string_view revspec{argv[idx]};
  const std::filesystem::path root =
      (idx + 1 < argc) ? std::filesystem::path{argv[idx + 1]} : std::filesystem::current_path();
  return run_diff(revspec, root, mode);
}

int dispatch_drift_baseline(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --drift-baseline requires <file>\n";
    return 2;
  }
  const std::filesystem::path driftFile{argv[2]};
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return run_check(root, OutputFormat::Text, {BaselineMode::None, {}, driftFile});
}

int dispatch_save_graph_baseline(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --save-graph-baseline requires <file>\n";
    return 2;
  }
  const std::filesystem::path file{argv[2]};
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return run_save_graph_baseline(root, file);
}

int dispatch_baseline(std::string_view arg, int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: " << arg << " requires <file>\n";
    return 2;
  }
  const BaselineMode mode = (arg == "--save-baseline") ? BaselineMode::Save : BaselineMode::Load;
  const std::filesystem::path file{argv[2]};
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return run_check(root, OutputFormat::Text, {mode, file, {}});
}

int dispatch_config(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --config requires <path>\n";
    return 2;
  }
  archcheck::config::Config config;
  try
  {
    config = archcheck::config::load(argv[2]);
  }
  catch (const archcheck::config::ConfigError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return 2;
  }
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return run_check(root, OutputFormat::Text, {}, config);
}

int dispatch_format(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --format requires a value (text|json)\n";
    return 2;
  }
  const std::string_view fmt{argv[2]};
  if (fmt != "json" && fmt != "text")
  {
    std::cerr << "archcheck: unknown format '" << fmt << "' (expected text|json)\n";
    return 2;
  }
  const auto format = (fmt == "json") ? OutputFormat::Json : OutputFormat::Text;
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return run_check(root, format);
}

bool requireDirectory(const std::filesystem::path &path)
{
  if (std::filesystem::is_directory(path))
    return true;
  std::cerr << "archcheck: not a directory: " << path.string() << '\n';
  return false;
}

int dispatch_with_path(std::string_view arg, int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: " << arg << " requires <path>\n";
    return 2;
  }
  if (!requireDirectory(argv[2]))
    return 2;
  if (arg == "--scan")
    return run_scan(argv[2]);
  if (arg == "--duplication")
    return run_duplication(argv[2]);
  if (arg == "--history")
    return run_history(argv[2]);
  return run_graph(argv[2]);
}

bool dispatchBuiltins(std::string_view arg)
{
  if (arg == "--version" || arg == "-V")
  {
    print_version();
    return true;
  }
  if (arg == "--help" || arg == "-h")
  {
    print_help();
    return true;
  }
  return false;
}

// Modes taking a single <path> argument, dispatched through dispatch_with_path.
bool isPathPreviewMode(std::string_view arg)
{
  return arg == "--scan" || arg == "--graph" || arg == "--duplication" || arg == "--history";
}

int dispatch(int argc, char *argv[])
{
  const std::string_view arg{argv[1]};
  if (dispatchBuiltins(arg))
    return 0;
  if (arg == "--baseline" || arg == "--save-baseline")
    return dispatch_baseline(arg, argc, argv);
  if (arg == "--drift-baseline")
    return dispatch_drift_baseline(argc, argv);
  if (arg == "--save-graph-baseline")
    return dispatch_save_graph_baseline(argc, argv);
  if (isPathPreviewMode(arg))
    return dispatch_with_path(arg, argc, argv);
  if (arg == "--diff")
    return dispatch_diff(argc, argv);
  if (arg == "--format")
    return dispatch_format(argc, argv);
  if (arg == "--config")
    return dispatch_config(argc, argv);
  if (!arg.empty() && arg[0] != '-')
  {
    const std::filesystem::path root{argv[1]};
    return requireDirectory(root) ? run_check(root, OutputFormat::Text) : 2;
  }
  std::cerr << "archcheck: unknown argument '" << arg << "'\n";
  print_help();
  return 2;
}

} // namespace

int main(int argc, char *argv[])
{
  try
  {
    if (argc < 2)
      return run_check(std::filesystem::current_path(), OutputFormat::Text);
    return dispatch(argc, argv);
  }
  catch (const std::exception &e)
  {
    std::cerr << "archcheck: internal error: " << e.what() << '\n';
    return 3;
  }
  catch (...)
  {
    std::cerr << "archcheck: internal error\n";
    return 3;
  }
}
