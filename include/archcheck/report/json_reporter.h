#pragma once

#include <ostream>

#include "archcheck/rules/violation.h"

namespace archcheck::report
{

void writeJsonReport(const rules::ViolationList &violations, std::ostream &out);

} // namespace archcheck::report
