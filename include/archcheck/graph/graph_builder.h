#pragma once

#include <cstddef>
#include <filesystem>

#include "archcheck/graph/dependency_graph.h"

namespace archcheck::scan
{
class FileSource;
class SourceSnapshot;
} // namespace archcheck::scan

namespace archcheck::graph
{

struct GraphBuildCounters
{
  std::size_t edges = 0;
  std::size_t external = 0;
  std::size_t unresolved = 0;
  std::size_t ambiguous = 0;
  std::size_t macro_includes = 0;
};

struct GraphBuildResult
{
  DependencyGraph graph;
  GraphBuildCounters counters;
};

// Build a DependencyGraph from any FileSource: list files, scan each for
// #includes, resolve targets against the project index, and assemble the
// graph. This is the source-of-truth implementation; `buildGraphForPath`
// is a thin wrapper that constructs a DiskFileSource and delegates here.
GraphBuildResult buildGraphForSource(scan::FileSource &source);

// Build a DependencyGraph from a pre-read+classified snapshot (#129): the
// single read-once stage. buildGraphForSource is a thin wrapper that reads a
// FileSource into a SourceSnapshot and delegates here. Rules that already hold
// a snapshot (the --diff orchestrator builds one per ref) call this directly so
// the tree is read once and shared across graph / clone / complexity.
GraphBuildResult buildGraphForSnapshot(const scan::SourceSnapshot &snapshot);

// End-to-end build over a real on-disk root. Same as buildGraphForSource
// with a DiskFileSource(root). Backs both `--graph` and `--diff` (the
// latter calls it once per side of the revspec).
GraphBuildResult buildGraphForPath(const std::filesystem::path &root);

} // namespace archcheck::graph
