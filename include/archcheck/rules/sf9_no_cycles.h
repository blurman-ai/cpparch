#pragma once

#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

class Sf9NoCycles final : public IRule
{
public:
  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "SF.9"; }
};

} // namespace archcheck::rules
