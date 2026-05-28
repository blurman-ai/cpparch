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

ViolationList scanFile(std::string_view path, const std::string &source)
{
  ViolationList result;
  int line = 0;
  std::size_t start = 0;
  while (start <= source.size())
  {
    const auto end = source.find('\n', start);
    const auto len = (end == std::string::npos) ? source.size() - start : end - start;
    ++line;
    const auto raw = std::string_view(source).substr(start, len);
    if (hasUsingNamespace(stripLineComment(raw)))
      result.push_back({"SF.7", std::string(path), line,
                        "'using namespace' at global scope in header (SF.7)"});
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
