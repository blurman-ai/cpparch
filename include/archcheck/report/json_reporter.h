#pragma once

#include <ostream>

#include "archcheck/rules/gate_policy.h"
#include "archcheck/rules/violation.h"

namespace archcheck::report
{

void writeJsonReport(const rules::ViolationList &violations, std::ostream &out, rules::GateMode gateMode);

} // namespace archcheck::report
