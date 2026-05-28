#pragma once

#include <memory>
#include <vector>

#include "archcheck/rules/i_rule.h"

namespace archcheck::rules
{

// Returns the default set of rules applied when archcheck runs without config.
std::vector<std::unique_ptr<IRule>> makeDefaultRuleSet();

} // namespace archcheck::rules
