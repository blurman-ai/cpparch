#include "archcheck/diff/md_report.h"

#include <ostream>

#include "archcheck/diff/regression_report.h"

namespace archcheck::diff
{

void writeMdReport(const RegressionReport &r, std::ostream &out)
{
  const char *icon = r.gates() ? ":x:" : (r.hasRegression() ? ":large_yellow_circle:" : ":white_check_mark:");
  out << "## archcheck `--diff`\n\n**Gate:** " << icon << " ";
  if (!r.grownCycles.empty())
    out << r.grownCycles.size() << " grown cycle(s)";
  else if (!r.newGodHeaders.empty())
    out << r.newGodHeaders.size() << " new god-header(s)";
  else if (!r.addedEdges.empty())
    out << r.addedEdges.size() << " added edge(s)";
  else if (!r.newCrossAreaDependencies.empty())
    out << r.newCrossAreaDependencies.size() << " new area dep(s)";
  else
    out << "no violations";
  out << "\n\n```\n";
  writeTextReport(r, out);
  out << "```\n";
}

} // namespace archcheck::diff
