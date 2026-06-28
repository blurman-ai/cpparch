#pragma once

#include <ostream>

#include "archcheck/diff/regression_report.h"

namespace archcheck::diff
{

void writeMdReport(const RegressionReport &r, std::ostream &out);

} // namespace archcheck::diff
