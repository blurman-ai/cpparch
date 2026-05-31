#include "archcheck/scan/include_resolver.h"

#include <array>
#include <string>
#include <string_view>

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

std::string source_directory(std::string_view sourceFile)
{
  const std::size_t slash = sourceFile.rfind('/');
  if (slash == std::string_view::npos)
  {
    return {};
  }
  return std::string{sourceFile.substr(0, slash + 1)};
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
  const std::string relative = source_directory(source) + d.token;
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
