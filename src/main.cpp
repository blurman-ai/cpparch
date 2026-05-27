#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "archcheck/graph/algorithms.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/scan/include_resolver.h"
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
            << "  archcheck --scan <path>    (preview: discover + scan #includes)\n"
            << "  archcheck --graph <path>   (preview: build dependency graph + SCC stats)\n"
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

struct GraphCounters
{
  std::size_t edges = 0;
  std::size_t external = 0;
  std::size_t unresolved = 0;
  std::size_t ambiguous = 0;
  std::size_t macro_includes = 0;
};

void apply_resolved(const std::vector<archcheck::scan::ResolvedInclude> &resolved, archcheck::graph::NodeId source,
                    const std::vector<archcheck::graph::NodeId> &id_map, archcheck::graph::DependencyGraph &dg,
                    GraphCounters &c)
{
  for (const auto &r : resolved)
  {
    switch (r.resolution)
    {
    case archcheck::scan::Resolution::Project:
      dg.add_edge(source, id_map[r.target]);
      ++c.edges;
      break;
    case archcheck::scan::Resolution::External:
      ++c.external;
      break;
    case archcheck::scan::Resolution::Unresolved:
      ++c.unresolved;
      break;
    case archcheck::scan::Resolution::Ambiguous:
      ++c.ambiguous;
      break;
    }
  }
}

struct GraphInputs
{
  const std::filesystem::path &root;
  const std::vector<archcheck::scan::ProjectFile> &files;
  const archcheck::scan::ProjectIndex &index;
  const std::vector<archcheck::graph::NodeId> &id_map;
};

void build_graph(const GraphInputs &in, archcheck::graph::DependencyGraph &dg, GraphCounters &c)
{
  for (std::size_t i = 0; i < in.files.size(); ++i)
  {
    const auto src = read_file(in.root / in.files[i].path);
    const auto scanned = archcheck::scan::scanIncludes(src);
    c.macro_includes += scanned.diagnostics.size();
    const auto resolved = archcheck::scan::resolveIncludes(scanned.directives, in.files[i].path, in.files, in.index);
    apply_resolved(resolved, in.id_map[i], in.id_map, dg, c);
  }
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
    {
      ++s.cyclic;
    }
    if (c.size() > s.largest)
    {
      s.largest = c.size();
    }
  }
  return s;
}

int run_graph(const std::filesystem::path &root)
{
  const auto files = archcheck::scan::discoverFiles(root);
  const auto index = archcheck::scan::buildProjectIndex(files);
  archcheck::graph::DependencyGraph dg;
  std::vector<archcheck::graph::NodeId> id_map;
  id_map.reserve(files.size());
  for (const auto &f : files)
  {
    id_map.push_back(dg.add_node(f.path));
  }
  GraphCounters c;
  build_graph(GraphInputs{root, files, index, id_map}, dg, c);
  const auto scc = compute_scc_stats(dg);
  std::cout << "nodes:          " << dg.node_count() << '\n'
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

int dispatch_with_path(const std::string_view &arg, int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: " << arg << " requires <path>\n";
    return 2;
  }
  if (arg == "--scan")
  {
    return run_scan(argv[2]);
  }
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
  {
    return dispatch_with_path(arg, argc, argv);
  }
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
