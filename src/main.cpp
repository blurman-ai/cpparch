#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "archcheck/diff/regression_report.h"
#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"
#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"
#include "archcheck/version.h"

namespace
{

void print_version() { std::cout << "archcheck " << archcheck::kVersionString << '\n'; }

void print_help()
{
  std::cout << "archcheck - architecture rules for C++ projects\n"
            << "\n"
            << "Usage:\n"
            << "  archcheck --version\n"
            << "  archcheck --help\n"
            << "  archcheck --scan  <path>             (preview: discover + scan #includes)\n"
            << "  archcheck --graph <path>             (preview: build dependency graph + SCC stats)\n"
            << "  archcheck --diff  [--diff-mode=disk|memory] <revspec> [path]\n"
            << "                                       (regression vs git ref; revspec = 'a..b' or '<ref>')\n"
            << "\n"
            << "  --diff-mode=memory (default) reads git blobs in-process via `git cat-file --batch`.\n"
            << "  --diff-mode=disk falls back to materialising each ref in a temporary worktree.\n"
            << "\n"
            << "Configuration parsing, default rules, and reporters land in subsequent\n"
            << "v0.1 commits. See docs/architecture-spec.md.\n";
}

std::string read_file(const std::filesystem::path &p)
{
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

int dispatch_with_path(std::string_view arg, int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: " << arg << " requires <path>\n";
    return 2;
  }
  if (arg == "--scan")
    return run_scan(argv[2]);
  return run_graph(argv[2]);
}

int dispatch(int argc, char *argv[])
{
  const std::string_view arg{argv[1]};
  if (arg == "--version" || arg == "-V")
  {
    print_version();
    return 0;
  }
  if (arg == "--help" || arg == "-h")
  {
    print_help();
    return 0;
  }
  if (arg == "--scan" || arg == "--graph")
    return dispatch_with_path(arg, argc, argv);
  if (arg == "--diff")
    return dispatch_diff(argc, argv);
  std::cerr << "archcheck: unknown argument '" << arg << "'\n";
  print_help();
  return 2;
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_help();
    return 0;
  }
  return dispatch(argc, argv);
}
