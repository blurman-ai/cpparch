#include "archcheck/scan/satd_scan.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <string_view>

#include "archcheck/scan/file_classification.h"

namespace archcheck::scan
{

namespace
{

std::string extractBlockCommentContent(std::string_view line, std::size_t startPos)
{
  // Find closing */ after /* at startPos
  for (std::size_t j = startPos + 2; j + 1 < line.size(); ++j)
  {
    if (line[j] == '*' && line[j + 1] == '/')
      return std::string{line.substr(startPos + 2, j - (startPos + 2))};
  }
  return "";
}

// Extract comment part of a line. Returns the text after // or inside /* */.
// This is a conservative approach: finds the first comment (// or /* ... */)
// outside of string literals. Returns empty string if no comment found.
std::string extractCommentPart(std::string_view line)
{
  bool inDoubleQuote = false;
  bool inSingleQuote = false;

  for (std::size_t i = 0; i < line.size(); ++i)
  {
    const char c = line[i];
    const char next = (i + 1 < line.size()) ? line[i + 1] : '\0';

    // Track string literals (simple escape handling)
    if (c == '"' && (i == 0 || line[i - 1] != '\\'))
      inDoubleQuote = !inDoubleQuote;
    else if (c == '\'' && (i == 0 || line[i - 1] != '\\'))
      inSingleQuote = !inSingleQuote;

    // Skip if inside quotes
    if (inDoubleQuote || inSingleQuote)
      continue;

    // Check for line comment //
    if (c == '/' && next == '/')
      return std::string{line.substr(i + 2)};

    // Check for block comment /* ... */
    if (c == '/' && next == '*')
      return extractBlockCommentContent(line, i);
  }

  return "";
}

bool hasSatd1Uppercase(std::string_view text)
{
  static const std::regex kUppercasePattern{R"(\b(TODO|FIXME|HACK|XXX|TEMP)\b)"};
  return std::regex_search(text.begin(), text.end(), kUppercasePattern);
}

bool hasSatd1Lowercase(std::string_view text)
{
  static const std::regex kLowercasePattern{R"(\b(temporary|workaround|dirty)\b|quick.?fix)", std::regex::icase};
  return std::regex_search(text.begin(), text.end(), kLowercasePattern);
}

// Check if text contains SATD.1 markers (any SATD keyword).
// Returns the rule id if found: "SATD.1" for new SATD, or empty string if not.
std::string checkSatd1(std::string_view text)
{
  if (hasSatd1Uppercase(text) || hasSatd1Lowercase(text))
    return "SATD.1";
  return "";
}

// Check if text contains SATD.2 (FIXME or HACK without issue id).
// Returns "SATD.2" if violation found, empty string otherwise.
std::string checkSatd2(std::string_view text)
{
  // Check for FIXME or HACK (case-sensitive, uppercase only)
  static const std::regex kFixmeOrHack{R"(\b(FIXME|HACK)\b)"};

  if (!std::regex_search(text.begin(), text.end(), kFixmeOrHack))
    return "";

  // Check if issue id is present
  static const std::regex kIssueId{R"(([A-Z][A-Z0-9]+-\d+)|(#\d+)|(\b(gh|issue)[-/]\d+))"};

  if (std::regex_search(text.begin(), text.end(), kIssueId))
    return "";

  return "SATD.2";
}

// Truncate message to 120 characters, preserving line endings.
std::string truncateMessage(std::string_view msg, std::size_t maxLen = 120)
{
  if (msg.size() <= maxLen)
    return std::string{msg};

  // Find last space/tab before maxLen to break at word boundary
  std::size_t breakPoint = maxLen;
  while (breakPoint > 0 && msg[breakPoint] != ' ' && msg[breakPoint] != '\t')
    breakPoint--;

  if (breakPoint == 0)
    breakPoint = maxLen;

  auto result = std::string{msg.substr(0, breakPoint)};

  // Strip trailing whitespace
  while (!result.empty() &&
         (result.back() == ' ' || result.back() == '\t' || result.back() == '\n' || result.back() == '\r'))
    result.pop_back();

  return result;
}

} // namespace

rules::ViolationList detectSatdMarkers(const std::vector<git::AddedLine> &addedLines)
{
  rules::ViolationList violations;

  for (const auto &added : addedLines)
  {
    // Vendored third-party and test code carry their own TODO/FIXME — that is not
    // the project's production self-admitted debt. Skip both, matching the
    // complexity-drift scan's classification (file_classification.h).
    const auto base = baseName(added.file);
    if (pathHasVendoredDir(added.file) || isVendoredBasename(base) || pathHasTestDir(added.file) ||
        isTestBasename(base))
      continue;

    const auto commentPart = extractCommentPart(added.text);
    if (commentPart.empty())
      continue;

    // SATD.2 (FIXME/HACK without issue id) is more specific — check it first.
    const auto satd2 = checkSatd2(commentPart);
    if (!satd2.empty())
    {
      violations.push_back({satd2, added.file, added.lineNumber, truncateMessage(added.text)});
      continue;
    }

    const auto satd1 = checkSatd1(commentPart);
    if (!satd1.empty())
      violations.push_back({satd1, added.file, added.lineNumber, truncateMessage(added.text)});
  }

  return violations;
}

} // namespace archcheck::scan
