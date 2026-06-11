#include "cli/preview_commands.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>

#include "archcheck/git/history_query.h"
#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/defect_attractor.h"
#include "archcheck/scan/disk_file_source.h"
#include "archcheck/scan/duplication/duplication_scanner.h"
#include "archcheck/scan/god_file_growth.h"
#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::cli
{

namespace
{

// S4: upper bound on individual source file size to prevent OOM.
constexpr std::size_t kMaxFileSizeBytes = 64ULL * 1024 * 1024; // 64 MiB

std::string readFileCapped(const std::filesystem::path &p)
{
  std::error_code ec;
  const auto sz = std::filesystem::file_size(p, ec);
  if (!ec && sz > kMaxFileSizeBytes)
  {
    std::cerr << "archcheck: skipping oversized file (> 64 MiB): " << p.string() << '\n';
    return {};
  }
  std::ifstream f(p, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

struct SccStats
{
  std::size_t total = 0;
  std::size_t cyclic = 0;
  std::size_t largest = 0;
};

SccStats computeSccStats(const archcheck::graph::DependencyGraph &dg)
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

std::map<std::string, std::int32_t> buildLocMap(const std::filesystem::path &root)
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

} // namespace

int runScan(const std::filesystem::path &root)
{
  const auto files = archcheck::scan::discoverFiles(root);
  std::size_t directives = 0;
  std::size_t quotes = 0;
  std::size_t angles = 0;
  std::size_t diagnostics = 0;
  for (const auto &f : files)
  {
    const auto res = archcheck::scan::scanIncludes(readFileCapped(root / f.path));
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

int runGraph(const std::filesystem::path &root)
{
  const auto built = archcheck::graph::buildGraphForPath(root);
  const auto &c = built.counters;
  const auto scc = computeSccStats(built.graph);
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

int runDuplication(const std::filesystem::path &root)
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

int runHistory(const std::filesystem::path &root)
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

} // namespace archcheck::cli
