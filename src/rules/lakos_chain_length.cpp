#include "archcheck/rules/lakos_chain_length.h"

#include <string>

#include "archcheck/graph/algorithms.h"

namespace archcheck::rules
{

ViolationList LakosChainLength::check(const graph::DependencyGraph &graph,
                                      const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  ViolationList result;
  const auto depths = archcheck::graph::computeIncludeDepths(graph);
  const auto count = static_cast<std::uint32_t>(graph.nodeCount());
  for (std::uint32_t i = 0; i < count; ++i)
  {
    if (depths[i] > threshold_)
    {
      result.push_back({"Lakos.ChainLength", std::string(graph.pathOf(graph::NodeId{i})), 0,
                        "include chain depth " + std::to_string(depths[i]) + " exceeds threshold " +
                            std::to_string(threshold_)});
    }
  }
  return result;
}

} // namespace archcheck::rules
