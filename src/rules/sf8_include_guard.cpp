#include "archcheck/rules/sf8_include_guard.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "archcheck/scan/project_files.h"

namespace archcheck::rules
{

namespace
{

// Scan first kScanLines non-empty lines for #pragma once or #ifndef guard.
// 60 covers Apache 2.0 copyright blocks (~47 lines) common in OSS headers.
constexpr int kScanLines = 60;

bool hasPragmaOnce(std::string_view line)
{
  const auto p = line.find("pragma");
  return p != std::string_view::npos && line.find("once", p) != std::string_view::npos;
}

bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

// Macro name following `directive` on `line`; empty if the directive is absent.
std::string_view directiveArg(std::string_view line, std::string_view directive)
{
  const auto p = line.find(directive);
  if (p == std::string_view::npos)
    return {};
  std::size_t b = p + directive.size();
  while (b < line.size() && (line[b] == ' ' || line[b] == '\t'))
    ++b;
  std::size_t e = b;
  while (e < line.size() && isIdentChar(line[e]))
    ++e;
  return line.substr(b, e - b);
}

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

// .inc files are platform-specific fragments included exactly once via #if.
// An include guard would contradict their purpose; skip SF.8 for them.
bool isIncFile(std::string_view path) { return std::filesystem::path(path).extension() == ".inc"; }

// Records an `#ifndef NAME` into `pending`; true when `line` is a `#define`
// matching a pending name — i.e. the pair completes a real include guard.
bool closesGuardPair(std::string_view line, std::vector<std::string_view> &pending)
{
  const auto name = directiveArg(line, "#ifndef");
  if (!name.empty())
  {
    pending.push_back(name);
    return false;
  }
  const auto defined = directiveArg(line, "#define");
  return !defined.empty() && std::find(pending.begin(), pending.end(), defined) != pending.end();
}

// A real include guard is the pair `#ifndef NAME` + later `#define NAME` of
// the same macro. A lone `#ifndef` (e.g. an NDEBUG tweak) has no guard
// semantics and must not satisfy SF.8.
bool hasIncludeGuard(const std::string &source)
{
  std::vector<std::string_view> pending; // #ifndef names awaiting their #define
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
      if (hasPragmaOnce(line) || closesGuardPair(line, pending))
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
    if (isIncFile(path))
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
