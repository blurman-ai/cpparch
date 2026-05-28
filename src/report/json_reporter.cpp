#include "archcheck/report/json_reporter.h"

#include <ostream>
#include <unordered_map>

namespace archcheck::report
{

namespace
{

// Minimal JSON string escaping (handles the characters that appear in paths/messages).
std::string jsonEscape(const std::string &s)
{
  std::string out;
  out.reserve(s.size());
  for (const char c : s)
  {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (c == '\n')
      out += "\\n";
    else
      out += c;
  }
  return out;
}

} // namespace

void writeJsonReport(const rules::ViolationList &violations, std::ostream &out)
{
  std::unordered_map<std::string, std::size_t> byRule;
  for (const auto &v : violations)
    ++byRule[v.ruleId];

  out << "{\n  \"version\": 1,\n  \"violations\": [\n";
  for (std::size_t i = 0; i < violations.size(); ++i)
  {
    const auto &v = violations[i];
    out << "    {\"rule\": \"" << jsonEscape(v.ruleId) << "\", \"file\": \"" << jsonEscape(v.file) << "\", \"line\": "
        << v.line << ", \"message\": \"" << jsonEscape(v.message) << "\"}";
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
