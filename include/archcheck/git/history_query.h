#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace archcheck::git
{

// A single file change from git log --numstat output.
struct FileChange
{
  std::string path;         // repository-relative path
  std::int32_t added = 0;   // number of added lines
  std::int32_t deleted = 0; // number of deleted lines
};

// A single commit from git log --numstat output.
struct CommitStats
{
  std::string sha;               // commit hash
  std::string subject;           // commit subject (first line of message)
  std::vector<FileChange> files; // file changes in this commit
};

// Query git log --numstat to collect commit-level statistics.
// Returns commits in git-log order (newest first) — detectors rely on this.
// Rename entries (with "=>") are skipped.
// Returns empty vector if git fails or produces no output.
// Respects limit (default 200) as --numstat records, not wall-clock time.
[[nodiscard]] std::vector<CommitStats> queryCommitHistory(const std::filesystem::path &repoRoot,
                                                          std::size_t limit = 200);

// Parser helper: converts git log --numstat output to CommitStats.
// Input format: lines of "added\tdeleted\tpath" separated by blank lines,
// with each block preceded by "sha\037subject" (ASCII 31 = unit separator).
// Rename entries ("old => new") are skipped. Used in tests and in queryCommitHistory.
[[nodiscard]] std::vector<CommitStats> parseHistoryOutput(std::string_view output);

} // namespace archcheck::git
