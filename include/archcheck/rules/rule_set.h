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

// Returns the drift rules — gating DRIFT.1 (shortcut edge) and DRIFT.2 (cycle
// growth) plus advisory DRIFT.3 (bidirectional coupling) — each holding a copy
// of the baseline graph. Call only when a graph baseline has been loaded.
std::vector<std::unique_ptr<IRule>> makeDriftRuleSet(graph::DependencyGraph baseline);

} // namespace archcheck::rules
