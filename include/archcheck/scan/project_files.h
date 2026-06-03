#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
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

// Same extension predicate as discoverFiles, exposed so callers (e.g. the
// git-diff fast-path) can pre-filter externally-sourced path lists without
// re-implementing the canonical set.
bool hasProjectExtension(const std::filesystem::path &p);

// Returns true for header-only extensions (.h .hh .hpp .hxx .ipp .tpp .inl .inc).
// Used by text-scan rules (SF.7, SF.8) that apply only to headers.
bool isHeaderFile(const std::filesystem::path &p);

// Build exact-path + '/'-segment suffix indexes over `files`. The NodeId
// of a file equals its index in the input vector.
ProjectIndex buildProjectIndex(const std::vector<ProjectFile> &files);

class FileSource; // defined in file_source.h

// Read all project files from `source`, dropping vendored code (vendored
// directory segments, vendored basenames, or vendor license headers), unit and
// integration test code (#070: test/ tests/ dirs, *_test/*_spec basenames), and
// empty files — the same exclusion set the graph builder applies. All three are
// noise in every signal, including duplication. Returns (repo-relative path, content).
std::vector<std::pair<std::string, std::string>> collectNonVendoredSources(FileSource &source);

} // namespace archcheck::scan
