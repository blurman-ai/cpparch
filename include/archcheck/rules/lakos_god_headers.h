#pragma once

#include <string>
#include <unordered_set>

#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

class LakosGodHeaders final : public IRule
{
public:
  static constexpr std::size_t kDefaultThreshold = 50;

  explicit LakosGodHeaders(std::size_t threshold = kDefaultThreshold,
                           std::unordered_set<std::string> extraExcludes = {})
      : threshold_(threshold), extraExcludes_(std::move(extraExcludes))
  {
  }

  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "Lakos.GodHeader"; }

private:
  static bool isPchName(std::string_view path);

  std::size_t threshold_;
  std::unordered_set<std::string> extraExcludes_;
};

} // namespace archcheck::rules
