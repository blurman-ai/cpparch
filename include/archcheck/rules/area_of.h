#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace archcheck::rules
{

namespace detail
{

inline const std::unordered_set<std::string> &wrapperDirs()
{
  static const std::unordered_set<std::string> kDirs = {"src",      "source", "sources", "include",
                                                        "includes", "inc",    "lib",     "libs"};
  return kDirs;
}

inline const std::unordered_set<std::string> &noiseDirs()
{
  static const std::unordered_set<std::string> kDirs = {"build",
                                                        "_build",
                                                        "out",
                                                        "cmake-build-debug",
                                                        "cmake-build-release",
                                                        "vendor",
                                                        "third_party",
                                                        "thirdparty",
                                                        "external",
                                                        "externals",
                                                        "_deps",
                                                        "extern",
                                                        "test",
                                                        "tests",
                                                        "testing",
                                                        "examples",
                                                        "example",
                                                        "generated",
                                                        "node_modules",
                                                        "contrib",
                                                        "mock",
                                                        "mocks"};
  return kDirs;
}

inline std::string lowerStr(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Build/test scaffolding segment — not a real module boundary.
inline bool isNoiseSeg(const std::string &seg)
{
  const std::string l = lowerStr(seg);
  if (noiseDirs().count(l))
    return true;
  if (l.rfind("build_", 0) == 0 || l.rfind("build-", 0) == 0 || l.rfind("mock", 0) == 0)
    return true;
  return l.find("override") != std::string::npos;
}

} // namespace detail

// Module name for a file path: first path component after stripping wrapper dirs.
// Returns empty string for root-level files and files under noise dirs.
inline std::string areaOf(std::string_view path)
{
  std::vector<std::string> seg;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= path.size(); ++i)
  {
    if (i == path.size() || path[i] == '/')
    {
      if (i > start)
        seg.emplace_back(path.substr(start, i - start));
      start = i + 1;
    }
  }
  for (const auto &s : seg)
    if (detail::isNoiseSeg(s))
      return {};
  std::size_t i = 0;
  while (i + 1 < seg.size() && detail::wrapperDirs().count(detail::lowerStr(seg[i])))
    ++i;
  if (i + 1 >= seg.size())
    return {};
  return seg[i];
}

} // namespace archcheck::rules
