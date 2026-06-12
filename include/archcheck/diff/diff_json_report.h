#pragma once

#include <ostream>
#include <string>

#include "archcheck/diff/regression_report.h"
#include "archcheck/rules/violation.h"

namespace archcheck::diff
{

// Run metadata + advisory scan findings (SATD, test co-evolution, local
// complexity) that accompany the structural RegressionReport in JSON output.
struct DiffJsonContext
{
  std::string baselineRef;
  std::string currentRef;
  rules::ViolationList advisoryViolations;
};

// Stable JSON document for `--diff --format=json`. Schema version 1:
// {version, baseline_ref, current_ref, gate, gating{...}, advisory{...}}.
// `gating` mirrors RegressionReport::gates(); everything else is advisory.
void writeJsonReport(const RegressionReport &r, const DiffJsonContext &ctx, std::ostream &out);

} // namespace archcheck::diff
