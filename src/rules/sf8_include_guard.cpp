#include "archcheck/rules/sf8_include_guard.h"

#include <string>
#include <string_view>

#include "archcheck/scan/project_files.h"

namespace archcheck::rules
{

namespace
{

// Scan first kScanLines non-empty lines for #pragma once or #ifndef guard.
constexpr int kScanLines = 30;

bool hasPragmaOnce(std::string_view line)
{
  const auto p = line.find("pragma");
  return p != std::string_view::npos && line.find("once", p) != std::string_view::npos;
}

bool hasIfndefGuard(std::string_view line) { return line.find("#ifndef") != std::string_view::npos; }

// Objective-C files use #import and @interface — not C++ headers, skip SF.8.
bool isObjcFile(const std::string &source)
{
  std::size_t start = 0;
  int seen = 0;
  while (start <= source.size() && seen < kScanLines)
  {
    const auto end = source.find('\n', start);
    const auto len = (end == std::string::npos) ? source.size() - start : end - start;
    const auto line = std::string_view(source).substr(start, len);
    if (!line.empty())
    {
      ++seen;
      if (line.find("@interface") != std::string_view::npos || line.find("@implementation") != std::string_view::npos ||
          line.find("#import") != std::string_view::npos)
        return true;
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

bool hasIncludeGuard(const std::string &source)
{
  int seen = 0;
  std::size_t start = 0;
  while (start <= source.size() && seen < kScanLines)
  {
    const auto end = source.find('\n', start);
    const auto len = (end == std::string::npos) ? source.size() - start : end - start;
    const auto line = std::string_view(source).substr(start, len);
    if (!line.empty())
    {
      ++seen;
      if (hasPragmaOnce(line) || hasIfndefGuard(line))
        return true;
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

} // namespace

ViolationList Sf8IncludeGuard::check(const graph::DependencyGraph &graph,
                                     const std::function<std::string(std::string_view)> &readFile) const
{
  ViolationList result;
  const auto count = static_cast<std::uint32_t>(graph.nodeCount());
  for (std::uint32_t i = 0; i < count; ++i)
  {
    const graph::NodeId id{i};
    const auto path = graph.pathOf(id);
    if (!archcheck::scan::isHeaderFile(std::filesystem::path(path)))
      continue;
    const auto source = readFile(path);
    if (isObjcFile(source))
      continue;
    if (!hasIncludeGuard(source))
      result.push_back({"SF.8", std::string(path), 1, "header missing #pragma once or include guard (SF.8)"});
  }
  return result;
}

} // namespace archcheck::rules
