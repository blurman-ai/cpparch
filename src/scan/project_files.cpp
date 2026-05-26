#include "archcheck/scan/project_files.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <system_error>

namespace archcheck::scan
{

namespace
{

constexpr std::array<std::string_view, 12> kExtensions = {
   ".c", ".cc", ".cpp", ".cxx",
   ".h", ".hh", ".hpp", ".hxx",
   ".ipp", ".tpp", ".inl", ".inc",
};

constexpr std::array<std::string_view, 6> kExcludedExact = {
   ".git", "build", ".cache", ".idea", ".vscode", "out",
};

constexpr std::string_view kCmakeBuildPrefix = "cmake-build-";

bool has_project_extension(const std::filesystem::path& p)
{
   const std::string ext = p.extension().string();
   return std::find(kExtensions.begin(), kExtensions.end(), ext) != kExtensions.end();
}

bool is_excluded_dir_name(std::string_view name)
{
   if (std::find(kExcludedExact.begin(), kExcludedExact.end(), name) != kExcludedExact.end())
   {
      return true;
   }
   return name.size() >= kCmakeBuildPrefix.size()
      && name.compare(0, kCmakeBuildPrefix.size(), kCmakeBuildPrefix) == 0;
}

std::string to_posix(const std::filesystem::path& relative)
{
   std::string s = relative.generic_string();
   // generic_string already yields '/'-separated form on all platforms.
   return s;
}

} // namespace

std::vector<ProjectFile> discover_files(const std::filesystem::path& root)
{
   std::vector<ProjectFile> out;
   std::error_code ec;
   std::filesystem::recursive_directory_iterator it(root, ec);
   const std::filesystem::recursive_directory_iterator end;
   while (!ec && it != end)
   {
      const auto& entry = *it;
      if (entry.is_directory(ec) && is_excluded_dir_name(entry.path().filename().string()))
      {
         it.disable_recursion_pending();
         it.increment(ec);
         continue;
      }
      if (entry.is_regular_file(ec) && has_project_extension(entry.path()))
      {
         out.push_back(ProjectFile{to_posix(std::filesystem::relative(entry.path(), root, ec))});
      }
      it.increment(ec);
   }
   std::sort(out.begin(), out.end(), [](const ProjectFile& a, const ProjectFile& b) { return a.path < b.path; });
   return out;
}

namespace
{

void add_suffixes(const std::string& path, NodeId id, ProjectIndex& idx)
{
   std::size_t start = 0;
   while (start != std::string::npos)
   {
      idx.suffix_index[path.substr(start)].push_back(id);
      const std::size_t slash = path.find('/', start);
      start = (slash == std::string::npos) ? std::string::npos : slash + 1;
   }
}

} // namespace

ProjectIndex build_project_index(const std::vector<ProjectFile>& files)
{
   ProjectIndex idx;
   idx.exact_path_index.reserve(files.size());
   for (NodeId id = 0; id < files.size(); ++id)
   {
      const std::string& path = files[id].path;
      idx.exact_path_index.emplace(path, id);
      add_suffixes(path, id, idx);
   }
   return idx;
}

} // namespace archcheck::scan
