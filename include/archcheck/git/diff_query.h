#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace archcheck::git
{

// A line added to the repository (identified from unified diff).
struct AddedLine
{
  std::string file;   // repository-relative path
  int lineNumber = 0; // 1-based line number in the new file
  std::string text;   // the line content (including trailing newline if present)
};

// Collect added lines from unified diff (--unified=0) between two git refs.
// Supports both revspecs like "a..b" and "ref..WORKTREE" (comparing ref to the working tree).
// Returns empty vector if diff is empty or on git failure.
[[nodiscard]] std::vector<AddedLine> collectAddedLines(const std::filesystem::path &repoRoot,
                                                       const std::string &baselineRef, const std::string &currentRef);

// Statistics from git diff --numstat.
struct NumStat
{
  std::string file; // repository-relative path
  int added = 0;    // number of added lines
  int removed = 0;  // number of removed lines
};

// Parse git diff --numstat output between two refs.
// Returns empty vector if diff is empty or on git failure.
[[nodiscard]] std::vector<NumStat> collectNumstat(const std::filesystem::path &repoRoot, const std::string &baselineRef,
                                                  const std::string &currentRef);

// Total added lines of a diff — the bulk-import gate for advisories (#117):
// a diff adding more than thresholds.diff_max_added_lines is a source drop,
// not authored evolution.
[[nodiscard]] inline std::size_t totalAddedLines(const std::vector<NumStat> &stats)
{
  std::size_t total = 0;
  for (const NumStat &s : stats)
    total += static_cast<std::size_t>(s.added > 0 ? s.added : 0);
  return total;
}

} // namespace archcheck::git
