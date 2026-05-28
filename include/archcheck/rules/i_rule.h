#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/violation.h"

namespace archcheck::rules
{

class IRule
{
public:
  virtual ~IRule() = default;
  virtual ViolationList check(const graph::DependencyGraph &graph,
                              const std::function<std::string(std::string_view)> &readFile) const = 0;
  virtual std::string_view id() const = 0;
};

} // namespace archcheck::rules
