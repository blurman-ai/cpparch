#include "archcheck/report/json_reporter.h"

#include <ostream>
#include <unordered_map>

#include "archcheck/report/json_escape.h"

namespace archcheck::report
{

namespace
{

std::string_view dispositionName(rules::FindingDisposition disposition)
{
  return disposition == rules::FindingDisposition::Gating ? "gating" : "advisory";
}

} // namespace

void writeJsonReport(const rules::ViolationList &violations, std::ostream &out, rules::GateMode gateMode)
{
  std::unordered_map<std::string, std::size_t> byRule;
  for (const auto &v : violations)
    ++byRule[v.ruleId];

  const bool gates = rules::countGating(violations, gateMode) > 0;
  out << "{\n  \"version\": 1,\n  \"gate\": \"" << (gates ? "fail" : "ok") << "\",\n  \"violations\": [\n";
  for (std::size_t i = 0; i < violations.size(); ++i)
  {
    const auto &v = violations[i];
    const auto disposition = rules::classifyForGate(v.ruleId, gateMode);
    out << "    {\"rule\": \"" << jsonEscape(v.ruleId) << "\", \"file\": \"" << jsonEscape(v.file)
        << "\", \"line\": " << v.line << ", \"disposition\": \"" << dispositionName(disposition)
        << "\", \"message\": \"" << jsonEscape(v.message) << "\"}";
    if (i + 1 < violations.size())
      out << ',';
    out << '\n';
  }
  out << "  ],\n  \"summary\": {\"total\": " << violations.size() << ", \"by_rule\": {";
  bool first = true;
  for (const auto &[rule, cnt] : byRule)
  {
    if (!first)
      out << ", ";
    out << '"' << jsonEscape(rule) << "\": " << cnt;
    first = false;
  }
  out << "}}\n}\n";
}

} // namespace archcheck::report
