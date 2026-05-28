#include "archcheck/rules/lakos_god_headers.h"

#include <filesystem>
#include <string>
#include <unordered_set>

namespace archcheck::rules
{

bool LakosGodHeaders::isPchName(std::string_view path)
{
  static const std::unordered_set<std::string> kKnownPch = {"pch.h", "stdafx.h", "precompiled.h",
                                                            "precompiled_header.h"};
  const auto stem = std::filesystem::path(std::string(path)).filename().string();
  return kKnownPch.count(stem) != 0;
}

ViolationList LakosGodHeaders::check(const graph::DependencyGraph &graph,
                                     const std::function<std::string(std::string_view)> & /*readFile*/) const
{
  ViolationList result;
  const auto count = static_cast<std::uint32_t>(graph.nodeCount());
  for (std::uint32_t i = 0; i < count; ++i)
  {
    const graph::NodeId id{i};
    const auto path = graph.pathOf(id);
    const auto filename = std::filesystem::path(std::string(path)).filename().string();
    if (isPchName(path) || extraExcludes_.count(filename))
      continue;
    const auto fanIn = graph.predecessors(id).size();
    if (fanIn > threshold_)
    {
      result.push_back({"Lakos.GodHeader", std::string(path), 0,
                        "fan-in " + std::to_string(fanIn) + " exceeds threshold " + std::to_string(threshold_)});
    }
  }
  return result;
}

} // namespace archcheck::rules
