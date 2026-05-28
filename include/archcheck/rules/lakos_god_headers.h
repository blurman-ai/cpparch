#pragma once

#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

class LakosGodHeaders final : public IRule
{
public:
  static constexpr std::size_t kDefaultThreshold = 30;

  explicit LakosGodHeaders(std::size_t threshold = kDefaultThreshold) : threshold_(threshold) {}

  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "Lakos.GodHeader"; }

private:
  std::size_t threshold_;
};

} // namespace archcheck::rules
