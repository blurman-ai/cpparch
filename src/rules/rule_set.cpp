#include "archcheck/rules/rule_set.h"

#include "archcheck/rules/drift_bidirectional_coupling.h"
#include "archcheck/rules/drift_no_cycle_growth.h"
#include "archcheck/rules/drift_no_shortcut_edge.h"
#include "archcheck/rules/lakos_chain_length.h"
#include "archcheck/rules/lakos_god_headers.h"
#include "archcheck/rules/sf7_using_namespace.h"
#include "archcheck/rules/sf8_include_guard.h"
#include "archcheck/rules/sf9_no_cycles.h"

namespace archcheck::rules
{

std::vector<std::unique_ptr<IRule>> makeDefaultRuleSet(const config::Config &config)
{
  std::vector<std::unique_ptr<IRule>> rules;
  rules.push_back(std::make_unique<Sf9NoCycles>());
  rules.push_back(std::make_unique<Sf7UsingNamespace>());
  rules.push_back(std::make_unique<Sf8IncludeGuard>());
  rules.push_back(std::make_unique<LakosGodHeaders>(config.thresholds.godHeaderFanIn));
  rules.push_back(std::make_unique<LakosChainLength>(config.thresholds.chainLength));
  return rules;
}

std::vector<std::unique_ptr<IRule>> makeDriftRuleSet(graph::DependencyGraph baseline)
{
  std::vector<std::unique_ptr<IRule>> rules;
  rules.push_back(std::make_unique<DriftNoShortcutEdge>(baseline));
  rules.push_back(std::make_unique<DriftBidirectionalCoupling>(baseline));
  rules.push_back(std::make_unique<DriftNoCycleGrowth>(std::move(baseline)));
  return rules;
}

} // namespace archcheck::rules
