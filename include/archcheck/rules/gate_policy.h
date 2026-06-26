#pragma once

#include <cstddef>
#include <string_view>

#include "archcheck/rules/violation.h"

namespace archcheck::rules
{

enum class GateMode
{
  Check,
  Drift
};

enum class FindingDisposition
{
  Advisory,
  Gating
};

[[nodiscard]] FindingDisposition classifyForGate(std::string_view ruleId, GateMode mode) noexcept;
[[nodiscard]] bool isGating(std::string_view ruleId, GateMode mode) noexcept;
[[nodiscard]] std::size_t countGating(const ViolationList &violations, GateMode mode);

} // namespace archcheck::rules
