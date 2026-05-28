#include "archcheck/report/text_reporter.h"

#include <ostream>
#include <unordered_map>

namespace archcheck::report
{

void writeTextReport(const rules::ViolationList &violations, std::ostream &out)
{
  for (const auto &v : violations)
  {
    out << v.file;
    if (v.line > 0)
      out << ':' << v.line;
    out << ": [" << v.ruleId << "] " << v.message << '\n';
  }

  // Summary by rule
  std::unordered_map<std::string, std::size_t> byRule;
  for (const auto &v : violations)
    ++byRule[v.ruleId];

  out << '\n';
  if (violations.empty())
  {
    out << "No violations found.\n";
    return;
  }
  out << violations.size() << " violation(s)";
  out << " (";
  bool first = true;
  for (const auto &[rule, cnt] : byRule)
  {
    if (!first)
      out << ", ";
    out << rule << ": " << cnt;
    first = false;
  }
  out << ")\n";
}

} // namespace archcheck::report
