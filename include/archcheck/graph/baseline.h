#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <utility>

#include "archcheck/graph/dependency_graph.h"

namespace archcheck::graph
{

struct BaselineLoadError
{
  enum class Kind
  {
    ParseError,
    UnknownVersion,
    MalformedSchema,
    MissingField
  };

  Kind kind;
  std::string message;
  int line = 0;
};

// Serialize `g` as a deterministic YAML baseline into `out`.
// Nodes are sorted lexicographically; edges are sorted by (from_idx, to_idx)
// using post-sort node indexes.
void saveBaseline(const DependencyGraph &g, std::ostream &out);

// Try to load a baseline from `in`. On success returns the graph and a
// disengaged optional. On failure returns an empty graph and an engaged
// optional with the structured error.
std::pair<DependencyGraph, std::optional<BaselineLoadError>> loadBaseline(std::istream &in);

} // namespace archcheck::graph
