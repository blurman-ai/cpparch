#pragma once

#include <memory>
#include <vector>

#include "archcheck/config/config.h"
#include "archcheck/graph/dependency_graph.h"
#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// Returns the default set of rules, with thresholds taken from config (a
// default-constructed Config carries the embedded defaults).
std::vector<std::unique_ptr<IRule>> makeDefaultRuleSet(const config::Config &config = {});

// Returns drift rules for graph-baseline mode. Exit gating is owned by
// gate_policy.h so rule registration and CI severity stay separate.
std::vector<std::unique_ptr<IRule>> makeDriftRuleSet(graph::DependencyGraph baseline);

} // namespace archcheck::rules
