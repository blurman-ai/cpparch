#pragma once

#include <ostream>

#include "archcheck/rules/violation.h"

namespace archcheck::report
{

void writeTextReport(const rules::ViolationList &violations, std::ostream &out, bool useColor = false);

} // namespace archcheck::report
