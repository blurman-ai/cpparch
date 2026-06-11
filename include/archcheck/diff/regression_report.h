#pragma once

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/diff.h"

namespace archcheck::diff
{

struct AddedEdge
{
  std::string from;
  std::string to;
};

struct RemovedEdge
{
  std::string from;
  std::string to;
};

struct GrownCycle
{
  std::vector<std::string> members;
  std::size_t baselineSize = 0;
  std::size_t currentSize = 0;
};

struct MetricDelta
{
  std::size_t baseline = 0;
  std::size_t current = 0;
};

struct NewCrossAreaDependency
{
  std::string fromArea;
  std::string toArea;
  std::size_t edgeCount = 0;
  std::string sampleFrom;
  std::string sampleTo;
};

struct MetricThresholds
{
  // Must match config::Thresholds::godHeaderFanIn (config.h) — `check` and
  // `--diff` share one contract; equality is pinned by a test.
  std::size_t godHeaderFanIn = 50;
};

// Aggregate result of comparing two dependency graphs. All NodeIds are
// resolved to paths up-front so the report stays valid after the source
// graphs go out of scope.
struct RegressionReport
{
  std::vector<AddedEdge> addedEdges;
  std::vector<RemovedEdge> removedEdges;
  std::vector<GrownCycle> grownCycles;
  std::vector<NewCrossAreaDependency> newCrossAreaDependencies;
  std::optional<MetricDelta> chainLengthGrown; // set when current max depth > baseline
  std::vector<std::string> newGodHeaders;      // nodes crossing godHeaderFanIn threshold
  std::optional<double> nccdDelta;             // set when NCCD increased

  bool hasRegression() const
  {
    return !addedEdges.empty() || !grownCycles.empty() || !newCrossAreaDependencies.empty() ||
           chainLengthGrown.has_value() || !newGodHeaders.empty() || (nccdDelta.has_value() && *nccdDelta > 0.0);
  }
};

RegressionReport buildRegressionReport(const graph::DependencyGraph &baseline, const graph::DependencyGraph &current,
                                       MetricThresholds thresholds = {});

void writeTextReport(const RegressionReport &r, std::ostream &out);

} // namespace archcheck::diff
