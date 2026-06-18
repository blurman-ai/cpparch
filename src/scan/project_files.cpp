#include "archcheck/scan/project_files.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <utility>

#include "archcheck/scan/authored_scope.h"
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

// True for a dir the walk must not descend into: an excluded name, or *any*
// symlinked dir — a self/loop symlink (e.g. CodeQL's
// _codeql_detected_source_root -> .) would otherwise spin the iterator forever.
bool should_skip_dir(const std::filesystem::directory_entry &entry)
{
  std::error_code ec;
  if (!entry.is_directory(ec))
  {
    return false;
  }
  return entry.is_symlink(ec) || isExcludedDirName(entry.path().filename().string());
}

// True when a file entry is a symlink whose resolved target escapes root.
// Security: prevents reading /etc/passwd via evil.h -> /etc/passwd.
bool symlink_escapes_root(const std::filesystem::directory_entry &entry, const std::filesystem::path &root)
{
  std::error_code ec;
  if (!entry.is_symlink(ec) || ec)
  {
    return false;
  }
  const auto target = std::filesystem::weakly_canonical(entry.path(), ec);
  if (ec)
  {
    return true; // unresolvable → treat as escaping
  }
  const auto canon_root = std::filesystem::weakly_canonical(root, ec);
  if (ec)
  {
    return false;
  }
  const auto rel = std::filesystem::relative(target, canon_root, ec);
  if (ec)
  {
    return true;
  }
  // relative() returns a path starting with ".." when target is outside root.
  const std::string first = rel.begin() != rel.end() ? rel.begin()->string() : "";
  return first == "..";
}

} // namespace

std::vector<ProjectFile> discoverFiles(const std::filesystem::path &root)
{
  std::vector<ProjectFile> out;
  std::error_code ec;
  // skip_permission_denied: one unreadable subtree must not abort the whole walk.
  std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied,
                                                   ec);
  const std::filesystem::recursive_directory_iterator end;
  while (it != end)
  {
    const auto &entry = *it;
    std::error_code item_ec;
    if (should_skip_dir(entry))
    {
      it.disable_recursion_pending();
    }
    else if (entry.is_regular_file(item_ec) && !symlink_escapes_root(entry, root) &&
             has_project_extension(entry.path()))
    {
      // GCC8-COMPAT: std::filesystem::relative() follows symlinks on GCC 8,
      // returning the canonical target path instead of the entry name.
      // lexically_relative() is purely lexical and preserves symlink names.
      const auto rel = entry.path().lexically_relative(root);
      if (!rel.empty() && rel.begin()->string() != "..")
      {
        out.push_back(ProjectFile{to_posix(rel)});
      }
    }
    it.increment(ec);
    ec.clear(); // a per-entry increment error must not halt the remaining walk
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
  // Load once, classify through the shared #129 gate (same vendor/test/generated/
  // dominant-banner formula as graph + complexity). Delta vs the old open-coded
  // version: gains the >50% dominant-banner guard (self-licensed Apache repos are
  // no longer over-excluded -> clone recall up) and the generated-file exclusion.
  std::vector<std::pair<std::string, std::string>> loaded;
  for (const auto &f : source.list())
  {
    loaded.push_back({f.path, source.read(f.path)});
  }
  const auto scope = AuthoredScope::fromFiles(loaded);

  std::vector<std::pair<std::string, std::string>> out;
  for (auto &pc : loaded)
  {
    if (pc.second.empty() || scope.excluded(pc.first, pc.second))
    {
      continue; // empty, or vendored / test / generated / non-dominant-banner
    }
    out.push_back(std::move(pc));
  }
  return out;
}

} // namespace archcheck::scan
