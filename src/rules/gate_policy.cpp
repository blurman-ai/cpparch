#include "archcheck/rules/gate_policy.h"

#include <algorithm>

namespace archcheck::rules
{

FindingDisposition classifyForGate(std::string_view ruleId, GateMode mode) noexcept
{
  if (mode == GateMode::Check)
    return ruleId == "SF.9" ? FindingDisposition::Gating : FindingDisposition::Advisory;

  return (ruleId == "DRIFT.1" || ruleId == "DRIFT.2" || ruleId == "DRIFT.4.CYCLE")
             ? FindingDisposition::Gating
             : FindingDisposition::Advisory;
}

bool isGating(std::string_view ruleId, GateMode mode) noexcept
{
  return classifyForGate(ruleId, mode) == FindingDisposition::Gating;
}

std::size_t countGating(const ViolationList &violations, GateMode mode)
{
  return static_cast<std::size_t>(
      std::count_if(violations.begin(), violations.end(), [mode](const auto &v) { return isGating(v.ruleId, mode); }));
}

} // namespace archcheck::rules

