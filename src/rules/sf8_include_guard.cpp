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

// Scan the first kScanLines lines of *code* for a #pragma once or #ifndef
// guard. Comment and blank lines are stripped first (stripComment), so a long
// license/doc banner never consumes the budget — some headers carry a 100+ line
// banner above the guard (#128, nanovdb). 60 code lines is ample headroom.
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

// Returns `line` with comment content removed, tracking /* */ state across
// lines via `inBlock`. Collapsing comments to blanks keeps a long license
// banner from consuming the guard-scan budget (#128). Guard regions hold no
// string literals, so string-awareness is unnecessary here.
std::string stripComment(std::string_view line, bool &inBlock)
{
  std::string code;
  std::size_t i = 0;
  while (i < line.size())
  {
    if (inBlock)
    {
      const auto close = line.find("*/", i);
      if (close == std::string_view::npos)
        return code;
      i = close + 2;
      inBlock = false;
    }
    else if (line.compare(i, 2, "//") == 0)
      break;
    else if (line.compare(i, 2, "/*") == 0)
    {
      inBlock = true;
      i += 2;
    }
    else
      code.push_back(line[i++]);
  }
  return code;
}

bool isBlank(std::string_view s)
{
  return std::all_of(s.begin(), s.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
}

// Visits up to kScanLines comment-stripped, non-blank lines from the top of
// `source`, calling `accept` on each; returns true as soon as `accept` does.
template <typename Accept> bool scanLeadingCode(const std::string &source, Accept accept)
{
  bool inBlock = false;
  int seen = 0;
  std::size_t start = 0;
  while (start <= source.size() && seen < kScanLines)
  {
    const auto end = source.find('\n', start);
    const auto len = (end == std::string::npos) ? source.size() - start : end - start;
    const auto code = stripComment(std::string_view(source).substr(start, len), inBlock);
    if (!isBlank(code))
    {
      ++seen;
      if (accept(std::string_view(code)))
        return true;
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

// Objective-C files use #import and @interface — not C++ headers, skip SF.8.
bool isObjcFile(const std::string &source)
{
  return scanLeadingCode(source,
                         [](std::string_view line)
                         {
                           return line.find("@interface") != std::string_view::npos ||
                                  line.find("@implementation") != std::string_view::npos ||
                                  line.find("#import") != std::string_view::npos;
                         });
}

// .inc files are platform-specific fragments included exactly once via #if.
// An include guard would contradict their purpose; skip SF.8 for them.
bool isIncFile(std::string_view path) { return std::filesystem::path(path).extension() == ".inc"; }

// Records an `#ifndef NAME` into `pending`; true when `line` is a `#define`
// matching a pending name — i.e. the pair completes a real include guard.
// Names are owned (std::string): the scanned line is a transient buffer.
bool closesGuardPair(std::string_view line, std::vector<std::string> &pending)
{
  const auto name = directiveArg(line, "#ifndef");
  if (!name.empty())
  {
    pending.emplace_back(name);
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
  std::vector<std::string> pending; // #ifndef names awaiting their #define
  return scanLeadingCode(source,
                         [&](std::string_view line) { return hasPragmaOnce(line) || closesGuardPair(line, pending); });
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
