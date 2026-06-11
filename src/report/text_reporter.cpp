#include "archcheck/report/text_reporter.h"

#include <ostream>
#include <unordered_map>

namespace archcheck::report
{

namespace
{

constexpr std::string_view kColorRed = "\033[31m";
constexpr std::string_view kColorGreen = "\033[32m";
constexpr std::string_view kColorReset = "\033[0m";

void writeSummary(const rules::ViolationList &violations, std::ostream &out, bool useColor)
{
  std::unordered_map<std::string, std::size_t> byRule;
  for (const auto &v : violations)
    ++byRule[v.ruleId];
  if (useColor)
    out << kColorRed;
  out << violations.size() << " violation(s) (";
  bool first = true;
  for (const auto &[rule, cnt] : byRule)
  {
    if (!first)
      out << ", ";
    out << rule << ": " << cnt;
    first = false;
  }
  out << ")";
  if (useColor)
    out << kColorReset;
  out << '\n';
}

} // namespace

void writeTextReport(const rules::ViolationList &violations, std::ostream &out, bool useColor)
{
  for (const auto &v : violations)
  {
    out << v.file;
    if (v.line > 0)
      out << ':' << v.line;
    out << ": ";
    if (useColor)
      out << kColorRed;
    out << "[" << v.ruleId << "]";
    if (useColor)
      out << kColorReset;
    out << " " << v.message << '\n';
  }
  out << '\n';
  if (violations.empty())
  {
    if (useColor)
      out << kColorGreen;
    out << "No violations found.";
    if (useColor)
      out << kColorReset;
    out << '\n';
    return;
  }
  writeSummary(violations, out, useColor);
}

} // namespace archcheck::report
