#pragma once

#include <memory>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// Returns the default set of rules applied when archcheck runs without config.
std::vector<std::unique_ptr<IRule>> makeDefaultRuleSet();

// Returns DRIFT.1 and DRIFT.2 rules, each holding a copy of the baseline graph
// for comparison. Call only when a graph baseline has been loaded.
std::vector<std::unique_ptr<IRule>> makeDriftRuleSet(graph::DependencyGraph baseline);

} // namespace archcheck::rules
