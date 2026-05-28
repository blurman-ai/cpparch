#include "archcheck/report/violation_baseline.h"

#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace archcheck::report
{

namespace
{

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

std::string jsonUnescape(std::string_view s)
{
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i)
  {
    if (s[i] == '\\' && i + 1 < s.size())
    {
      ++i;
      if (s[i] == '"')
        out += '"';
      else if (s[i] == '\\')
        out += '\\';
      else if (s[i] == 'n')
        out += '\n';
      else
        out += s[i];
    }
    else
    {
      out += s[i];
    }
  }
  return out;
}

// Extract the JSON string value for `key` from a single-line violation object.
// Returns nullopt if the key is not found or the value is unterminated.
std::optional<std::string> extractString(std::string_view line, std::string_view key)
{
  const std::string needle = "\"" + std::string(key) + "\": \"";
  const auto pos = line.find(needle);
  if (pos == std::string_view::npos)
    return std::nullopt;
  std::size_t i = pos + needle.size();
  const std::size_t start = i;
  while (i < line.size())
  {
    if (line[i] == '\\')
      ++i; // skip escaped character
    else if (line[i] == '"')
      return jsonUnescape(line.substr(start, i - start));
    ++i;
  }
  return std::nullopt; // unterminated string
}

std::optional<int> extractInt(std::string_view line, std::string_view key)
{
  const std::string needle = "\"" + std::string(key) + "\": ";
  const auto pos = line.find(needle);
  if (pos == std::string_view::npos)
    return std::nullopt;
  std::size_t i = pos + needle.size();
  const bool neg = (i < line.size() && line[i] == '-');
  if (neg)
    ++i;
  int val = 0;
  bool found = false;
  while (i < line.size() && line[i] >= '0' && line[i] <= '9')
  {
    val = val * 10 + static_cast<int>(line[i] - '0');
    ++i;
    found = true;
  }
  if (!found)
    return std::nullopt;
  return neg ? -val : val;
}

bool isViolationLine(std::string_view line)
{
  return line.size() >= 5 && line[0] == ' ' && line[1] == ' ' && line[2] == ' ' && line[3] == ' ' && line[4] == '{';
}

rules::Violation parseViolationLine(std::string_view sv, int lineNo, const std::string &pathStr)
{
  auto rule = extractString(sv, "rule");
  auto file = extractString(sv, "file");
  auto ln = extractInt(sv, "line");
  auto message = extractString(sv, "message");
  if (!rule || !file || !ln || !message)
    throw BaselineError("malformed violation on line " + std::to_string(lineNo) + " of '" + pathStr + "'");
  return {std::move(*rule), std::move(*file), *ln, std::move(*message)};
}

} // namespace

void saveBaseline(const ViolationBaseline &baseline, const std::filesystem::path &path)
{
  std::ofstream f(path);
  if (!f)
    throw BaselineError("cannot open '" + path.string() + "' for writing");

  const auto &vs = baseline.known;
  f << "{\n  \"version\": 1,\n  \"violations\": [\n";
  for (std::size_t i = 0; i < vs.size(); ++i)
  {
    const auto &v = vs[i];
    f << "    {\"rule\": \"" << jsonEscape(v.ruleId) << "\", \"file\": \"" << jsonEscape(v.file)
      << "\", \"line\": " << v.line << ", \"message\": \"" << jsonEscape(v.message) << "\"}";
    if (i + 1 < vs.size())
      f << ',';
    f << '\n';
  }
  f << "  ]\n}\n";
}

ViolationBaseline loadBaseline(const std::filesystem::path &path)
{
  std::ifstream f(path);
  if (!f)
    throw BaselineError("cannot open '" + path.string() + "': file not found");

  const std::string content{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  if (content.find("\"violations\"") == std::string::npos)
    throw BaselineError("not a valid archcheck baseline: '" + path.string() + "'");

  ViolationBaseline result;
  std::istringstream ss(content);
  std::string line;
  int lineNo = 0;
  while (std::getline(ss, line))
  {
    ++lineNo;
    if (!isViolationLine(line))
      continue;
    result.known.push_back(parseViolationLine(line, lineNo, path.string()));
  }
  return result;
}

rules::ViolationList filterNew(const rules::ViolationList &all, const ViolationBaseline &baseline)
{
  std::set<std::tuple<std::string, std::string, int>> known;
  for (const auto &v : baseline.known)
    known.emplace(v.ruleId, v.file, v.line);

  rules::ViolationList result;
  for (const auto &v : all)
  {
    if (!known.count({v.ruleId, v.file, v.line}))
      result.push_back(v);
  }
  return result;
}

} // namespace archcheck::report
