#include "archcheck/rules/lakos_god_headers.h"

#include <string>

namespace archcheck::rules
{

ViolationList LakosGodHeaders::check(const graph::DependencyGraph &graph,
                                     const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  ViolationList result;
  const auto count = static_cast<std::uint32_t>(graph.nodeCount());
  for (std::uint32_t i = 0; i < count; ++i)
  {
    const graph::NodeId id{i};
    const auto fanIn = graph.predecessors(id).size();
    if (fanIn > threshold_)
    {
      result.push_back({"Lakos.GodHeader", std::string(graph.pathOf(id)), 0,
                        "fan-in " + std::to_string(fanIn) + " exceeds threshold " +
                            std::to_string(threshold_)});
    }
  }
  return result;
}

} // namespace archcheck::rules
