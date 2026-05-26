#include "archcheck/scan/include_resolver.h"

#include <string>
#include <string_view>

namespace archcheck::scan
{

namespace
{

std::string source_directory(std::string_view source_file)
{
   const std::size_t slash = source_file.rfind('/');
   if (slash == std::string_view::npos)
   {
      return {};
   }
   return std::string{source_file.substr(0, slash + 1)};
}

ResolvedInclude make_project(const IncludeDirective& d, std::string_view source, NodeId target)
{
   return ResolvedInclude{d, std::string{source}, Resolution::Project, target, {}};
}

ResolvedInclude make_ambiguous(const IncludeDirective& d, std::string_view source, std::vector<NodeId> ids)
{
   return ResolvedInclude{d, std::string{source}, Resolution::Ambiguous, NodeId{}, std::move(ids)};
}

ResolvedInclude make_tagged(const IncludeDirective& d, std::string_view source, Resolution r)
{
   return ResolvedInclude{d, std::string{source}, r, NodeId{}, {}};
}

const NodeId* find_exact(const ProjectIndex& index, const std::string& path)
{
   const auto it = index.exact_path_index.find(path);
   return (it == index.exact_path_index.end()) ? nullptr : &it->second;
}

const std::vector<NodeId>* find_suffix(const ProjectIndex& index, const std::string& token)
{
   const auto it = index.suffix_index.find(token);
   return (it == index.suffix_index.end()) ? nullptr : &it->second;
}

ResolvedInclude resolve_by_suffix(
   const IncludeDirective& d, std::string_view source, const ProjectIndex& index, Resolution miss)
{
   const std::vector<NodeId>* candidates = find_suffix(index, d.token);
   if (candidates == nullptr || candidates->empty())
   {
      return make_tagged(d, source, miss);
   }
   if (candidates->size() == 1)
   {
      return make_project(d, source, candidates->front());
   }
   return make_ambiguous(d, source, *candidates);
}

ResolvedInclude resolve_quote(
   const IncludeDirective& d, std::string_view source, const ProjectIndex& index)
{
   const std::string relative = source_directory(source) + d.token;
   if (const NodeId* hit = find_exact(index, relative))
   {
      return make_project(d, source, *hit);
   }
   if (const NodeId* hit = find_exact(index, d.token))
   {
      return make_project(d, source, *hit);
   }
   return resolve_by_suffix(d, source, index, Resolution::Unresolved);
}

ResolvedInclude resolve_angle(
   const IncludeDirective& d, std::string_view source, const ProjectIndex& index)
{
   if (const NodeId* hit = find_exact(index, d.token))
   {
      return make_project(d, source, *hit);
   }
   return resolve_by_suffix(d, source, index, Resolution::External);
}

} // namespace

ResolvedInclude resolve_include(
   const IncludeDirective& directive,
   std::string_view source_file,
   const std::vector<ProjectFile>& /*files*/,
   const ProjectIndex& index)
{
   if (directive.kind == IncludeKind::Quote)
   {
      return resolve_quote(directive, source_file, index);
   }
   return resolve_angle(directive, source_file, index);
}

std::vector<ResolvedInclude> resolve_includes(
   const std::vector<IncludeDirective>& directives,
   std::string_view source_file,
   const std::vector<ProjectFile>& files,
   const ProjectIndex& index)
{
   std::vector<ResolvedInclude> out;
   out.reserve(directives.size());
   for (const IncludeDirective& d : directives)
   {
      out.push_back(resolve_include(d, source_file, files, index));
   }
   return out;
}

} // namespace archcheck::scan
