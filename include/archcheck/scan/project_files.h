#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace archcheck::scan
{

using NodeId = std::size_t;

struct ProjectFile
{
  std::string path; // repo-relative, POSIX separators ('/')
};

struct ProjectIndex
{
  // exact repo-relative POSIX path -> file id
  std::unordered_map<std::string, NodeId> exactPathIndex;
  // every '/'-suffix of every project file -> candidate file ids
  std::unordered_map<std::string, std::vector<NodeId>> suffixIndex;
};

// Walk `root` recursively and collect project files matching the v0.1
// extension set (.c/.cc/.cpp/.cxx/.h/.hh/.hpp/.hxx/.ipp/.tpp/.inl/.inc).
// Skips directories listed in mini-design §1: .git, build, cmake-build-*,
// .cache, .idea, .vscode, out. Returned paths are repo-relative and use
// '/' as the path separator. The result is sorted for determinism.
std::vector<ProjectFile> discoverFiles(const std::filesystem::path &root);

// Build exact-path + '/'-segment suffix indexes over `files`. The NodeId
// of a file equals its index in the input vector.
ProjectIndex buildProjectIndex(const std::vector<ProjectFile> &files);

} // namespace archcheck::scan
