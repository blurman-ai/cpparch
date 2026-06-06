#include "archcheck/scan/include_resolver.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

namespace archcheck::scan
{

namespace
{

constexpr std::array<std::string_view, 6> kMirrorPrefixes = {"single_include/", "amalgamate/", "amalgamated/",
                                                             "dist/",           "generated/",  "release/include/"};

bool is_mirror_dir_path(std::string_view path)
{
  for (std::string_view prefix : kMirrorPrefixes)
  {
    // GCC8-COMPAT: starts_with() requires GCC 10; replace when Astra upgrades
    if (path.find(prefix) == 0) // cppcheck-suppress stlIfStrFind
      return true;
    std::string needle;
    needle.reserve(prefix.size() + 1);
    needle += '/';
    needle.append(prefix);
    if (path.find(needle) != std::string_view::npos)
      return true;
  }
  return false;
}

// A standard system header (`<string.h>`, `<vector>`, …) must resolve as External
// even when the project ships a file with the same basename (e.g. a local
// compat/string.h). Otherwise suffix-matching invents a phantom edge / cycle
// (#088: PipeWire defs.h <-> system <string.h>). Extensionless angle tokens are
// C++/system headers (project headers carry an extension); a curated set covers
// the standard C `*.h` family that most often collides with project basenames.
bool is_system_header(std::string_view token)
{
  if (token.find('/') != std::string_view::npos)
    return false; // <foo/bar.h> is a pathed (project or vendored) header, not bare system
  if (token.find('.') == std::string_view::npos)
    return true; // <vector>, <cstdint>, <memory> — extensionless => C++/system
  static const std::unordered_set<std::string_view> kStdCHeaders = {
      "assert.h",  "complex.h", "ctype.h",  "errno.h",  "fenv.h",   "float.h",       "inttypes.h", "iso646.h",
      "limits.h",  "locale.h",  "math.h",   "setjmp.h", "signal.h", "stdalign.h",    "stdarg.h",   "stdatomic.h",
      "stdbool.h", "stddef.h",  "stdint.h", "stdio.h",  "stdlib.h", "stdnoreturn.h", "string.h",   "tgmath.h",
      "threads.h", "time.h",    "uchar.h",  "wchar.h",  "wctype.h"};
  return kStdCHeaders.count(token) != 0;
}

std::string source_directory(std::string_view sourceFile)
{
  const std::size_t slash = sourceFile.rfind('/');
  if (slash == std::string_view::npos)
  {
    return {};
  }
  return std::string{sourceFile.substr(0, slash + 1)};
}

// Collapse "." / ".." segments so a relative include like "tests/../src/core.h"
// matches the indexed project path "src/core.h". Includes that escape the
// project root keep their leading ".." and simply fail to match (correct).
std::string normalize_relative(std::string_view path)
{
  return std::filesystem::path(path).lexically_normal().generic_string();
}

ResolvedInclude make_project(const IncludeDirective &d, std::string_view source, NodeId target)
{
  return ResolvedInclude{d, std::string{source}, Resolution::Project, target, {}};
}

ResolvedInclude make_ambiguous(const IncludeDirective &d, std::string_view source, std::vector<NodeId> ids)
{
  return ResolvedInclude{d, std::string{source}, Resolution::Ambiguous, NodeId{}, std::move(ids)};
}

ResolvedInclude make_tagged(const IncludeDirective &d, std::string_view source, Resolution r)
{
  return ResolvedInclude{d, std::string{source}, r, NodeId{}, {}};
}

const NodeId *find_exact(const ProjectIndex &index, const std::string &path)
{
  const auto it = index.exactPathIndex.find(path);
  return (it == index.exactPathIndex.end()) ? nullptr : &it->second;
}

const std::vector<NodeId> *find_suffix(const ProjectIndex &index, const std::string &token)
{
  const auto it = index.suffixIndex.find(token);
  return (it == index.suffixIndex.end()) ? nullptr : &it->second;
}

ResolvedInclude resolve_by_suffix(const IncludeDirective &d, std::string_view source,
                                  const std::vector<ProjectFile> &files, const ProjectIndex &index, Resolution miss)
{
  const std::vector<NodeId> *candidates = find_suffix(index, d.token);
  if (candidates == nullptr || candidates->empty())
  {
    return make_tagged(d, source, miss);
  }
  if (candidates->size() == 1)
  {
    return make_project(d, source, candidates->front());
  }
  std::vector<NodeId> preferred;
  for (NodeId id : *candidates)
    if (!is_mirror_dir_path(files[id].path))
      preferred.push_back(id);
  if (preferred.size() == 1)
    return make_project(d, source, preferred.front());
  return make_ambiguous(d, source, *candidates);
}

ResolvedInclude resolve_quote(const IncludeDirective &d, std::string_view source, const std::vector<ProjectFile> &files,
                              const ProjectIndex &index)
{
  const std::string relative = normalize_relative(source_directory(source) + d.token);
  if (const NodeId *hit = find_exact(index, relative))
  {
    return make_project(d, source, *hit);
  }
  if (const NodeId *hit = find_exact(index, d.token))
  {
    return make_project(d, source, *hit);
  }
  return resolve_by_suffix(d, source, files, index, Resolution::Unresolved);
}

ResolvedInclude resolve_angle(const IncludeDirective &d, std::string_view source, const std::vector<ProjectFile> &files,
                              const ProjectIndex &index)
{
  if (is_system_header(d.token))
  {
    return make_tagged(d, source, Resolution::External);
  }
  if (const NodeId *hit = find_exact(index, d.token))
  {
    return make_project(d, source, *hit);
  }
  return resolve_by_suffix(d, source, files, index, Resolution::External);
}

} // namespace

ResolvedInclude resolveInclude(const IncludeDirective &directive, std::string_view sourceFile,
                               const std::vector<ProjectFile> &files, const ProjectIndex &index)
{
  const ResolvedInclude resolved = (directive.kind == IncludeKind::Quote)
                                       ? resolve_quote(directive, sourceFile, files, index)
                                       : resolve_angle(directive, sourceFile, files, index);
  // A component never depends on itself. A same-basename header elsewhere on
  // the include path (e.g. system <cpuid.h> suffix-matching project .../cpuid.h)
  // must not create a self-edge / phantom 1-node cycle. Downgrade to the
  // not-found tag for the directive kind. Same class as #036, single-candidate
  // variant where the mirror-dir filter cannot fire.
  if (resolved.resolution == Resolution::Project && files[resolved.target].path == sourceFile)
  {
    const Resolution miss = (directive.kind == IncludeKind::Quote) ? Resolution::Unresolved : Resolution::External;
    return make_tagged(directive, sourceFile, miss);
  }
  return resolved;
}

std::vector<ResolvedInclude> resolveIncludes(const std::vector<IncludeDirective> &directives,
                                             std::string_view sourceFile, const std::vector<ProjectFile> &files,
                                             const ProjectIndex &index)
{
  std::vector<ResolvedInclude> out;
  out.reserve(directives.size());
  for (const IncludeDirective &d : directives)
  {
    out.push_back(resolveInclude(d, sourceFile, files, index));
  }
  return out;
}

} // namespace archcheck::scan
