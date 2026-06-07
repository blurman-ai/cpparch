#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

class ImplicitStateMachineGrowth : public IRule
{
public:
  ImplicitStateMachineGrowth(std::size_t minBoolFields = 5, double statePatternRatio = 0.6);

  ViolationList check(const graph::DependencyGraph &graph,
                      const std::function<std::string(std::string_view)> &readFile) const override;
  std::string_view id() const override { return "implicit_state_machine_growth"; }

private:
  struct BoolStruct
  {
    std::string structName;
    std::string filePath;
    std::size_t lineno;
    std::size_t numBoolFields;
    std::vector<std::string> boolFieldNames;
    std::size_t statePatternMatches;
  };

  std::size_t minBoolFields_;
  double statePatternRatio_;

  // Helper methods
  static bool isStatePatternName(std::string_view name);
  static bool isConfigPatternName(std::string_view name);
  static bool shouldExclude(std::string_view structName);

  // Scan source file for struct/class definitions and count bool fields
  std::vector<BoolStruct> scanSourceFile(std::string_view path, const std::string &source) const;
};

} // namespace archcheck::rules
