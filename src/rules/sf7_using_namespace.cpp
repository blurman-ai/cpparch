#include "archcheck/rules/sf7_using_namespace.h"

#include <string>
#include <string_view>

#include "archcheck/scan/project_files.h"

namespace archcheck::rules
{

namespace
{

// Strip the // line comment from a source line (approximate: ignores strings).
std::string_view stripLineComment(std::string_view line)
{
  const auto pos = line.find("//");
  return (pos == std::string_view::npos) ? line : line.substr(0, pos);
}

bool hasUsingNamespace(std::string_view line)
{
  const auto pos = line.find("using");
  if (pos == std::string_view::npos)
    return false;
  const auto after = line.find_first_not_of(" \t", pos + 5);
  return after != std::string_view::npos && line.compare(after, 9, "namespace") == 0;
}

// Returns the visible portion of `raw` and advances `inBlockComment` state.
// Approximate: for single-line /* ... */ returns only text before "/*".
std::string_view updateBlockCommentState(std::string_view raw, bool &inBlockComment)
{
  if (inBlockComment)
  {
    const auto close = raw.find("*/");
    if (close == std::string_view::npos)
      return {};
    inBlockComment = false;
    return raw.substr(close + 2);
  }
  const auto open = raw.find("/*");
  if (open == std::string_view::npos)
    return raw;
  inBlockComment = (raw.find("*/", open + 2) == std::string_view::npos);
  return raw.substr(0, open);
}

ViolationList scanFile(std::string_view path, const std::string &source)
{
  ViolationList result;
  int line = 0;
  int braceDepth = 0;
  bool inBlockComment = false;
  std::size_t start = 0;
  while (start <= source.size())
  {
    const auto end = source.find('\n', start);
    const auto len = (end == std::string::npos) ? source.size() - start : end - start;
    ++line;
    const auto vis = updateBlockCommentState(std::string_view(source).substr(start, len), inBlockComment);
    const auto noComment = stripLineComment(vis);
    for (const char c : noComment)
      if (c == '{')
        ++braceDepth;
    if (braceDepth == 0 && hasUsingNamespace(noComment))
      result.push_back({"SF.7", std::string(path), line, "'using namespace' at global scope in header (SF.7)"});
    for (const char c : noComment)
      if (c == '}')
        --braceDepth;
    if (braceDepth < 0)
      braceDepth = 0;
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return result;
}

} // namespace

ViolationList Sf7UsingNamespace::check(const graph::DependencyGraph &graph,
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
    auto v = scanFile(path, source);
    result.insert(result.end(), v.begin(), v.end());
  }
  return result;
}

} // namespace archcheck::rules
