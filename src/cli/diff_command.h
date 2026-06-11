#pragma once

#include <filesystem>
#include <string_view>

namespace archcheck::cli
{

enum class DiffMode
{
  Memory, // git cat-file --batch (default)
  Disk    // git worktree add (legacy fallback)
};

// Structural regression report vs a git ref, followed by advisory signals
// (SATD, test co-evolution, local complexity drift). Thresholds come from the
// .archcheck.yml discovered at the repo root. Exit 1 on regression, 2 on error.
int runDiff(std::string_view revspec, const std::filesystem::path &root, DiffMode mode);

} // namespace archcheck::cli
