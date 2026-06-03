#include "archcheck/scan/project_files.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <utility>

#include "archcheck/scan/file_classification.h"
#include "archcheck/scan/file_source.h"

namespace archcheck::scan
{

namespace
{

bool has_project_extension(const std::filesystem::path &p)
{
  const std::string ext = p.extension().string();
  return std::find(kProjectExtensions.begin(), kProjectExtensions.end(), ext) != kProjectExtensions.end();
}

bool is_header_file(const std::filesystem::path &p)
{
  const std::string ext = p.extension().string();
  return std::find(kHeaderExtensions.begin(), kHeaderExtensions.end(), ext) != kHeaderExtensions.end();
}

std::string to_posix(const std::filesystem::path &relative)
{
  std::string s = relative.generic_string();
  // generic_string already yields '/'-separated form on all platforms.
  return s;
}

} // namespace

std::vector<ProjectFile> discoverFiles(const std::filesystem::path &root)
{
  std::vector<ProjectFile> out;
  std::error_code ec;
  std::filesystem::recursive_directory_iterator it(root, ec);
  const std::filesystem::recursive_directory_iterator end;
  while (!ec && it != end)
  {
    const auto &entry = *it;
    if (entry.is_directory(ec) && isExcludedDirName(entry.path().filename().string()))
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
  std::sort(out.begin(), out.end(), [](const ProjectFile &a, const ProjectFile &b) { return a.path < b.path; });
  return out;
}

namespace
{

void add_suffixes(const std::string &path, NodeId id, ProjectIndex &idx)
{
  std::size_t start = 0;
  while (start != std::string::npos)
  {
    idx.suffixIndex[path.substr(start)].push_back(id);
    const std::size_t slash = path.find('/', start);
    start = (slash == std::string::npos) ? std::string::npos : slash + 1;
  }
}

} // namespace

ProjectIndex buildProjectIndex(const std::vector<ProjectFile> &files)
{
  ProjectIndex idx;
  idx.exactPathIndex.reserve(files.size());
  for (NodeId id = 0; id < files.size(); ++id)
  {
    const std::string &path = files[id].path;
    idx.exactPathIndex.emplace(path, id);
    add_suffixes(path, id, idx);
  }
  return idx;
}

bool hasProjectExtension(const std::filesystem::path &p) { return has_project_extension(p); }

bool isHeaderFile(const std::filesystem::path &p) { return is_header_file(p); }

std::vector<std::pair<std::string, std::string>> collectNonVendoredSources(FileSource &source)
{
  std::vector<std::pair<std::string, std::string>> out;
  for (const auto &f : source.list())
  {
    if (pathHasVendoredDir(f.path))
    {
      continue; // vendored directory segment (third_party/, vendor/, deps/, ...)
    }
    std::string content = source.read(f.path);
    if (content.empty() || isVendoredFile(baseName(f.path), content))
    {
      continue; // empty, or vendored basename / vendor license header
    }
    out.push_back({f.path, std::move(content)});
  }
  return out;
}

} // namespace archcheck::scan
