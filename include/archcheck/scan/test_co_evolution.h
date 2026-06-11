#pragma once

#include <vector>

#include "archcheck/git/diff_query.h"
#include "archcheck/rules/violation.h"

namespace archcheck::scan
{

// Detect test co-evolution issues: production code changing significantly while
// tests remain unchanged or change only minimally.
// Input: numstat entries from a git diff.
// Output: at most one Violation with ruleId="TEST.1.prod_changed_tests_silent" if
// production churn exceeds thresholds and test churn is insufficient.
[[nodiscard]] rules::ViolationList detectTestCoEvolution(const std::vector<archcheck::git::NumStat> &numstat);

} // namespace archcheck::scan
