#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace archcheck::git
{

// Result of a fork/exec'd git command.
struct GitRun
{
  int exitCode = -1;
  std::string out;
  std::string err;
};

// Fork/exec a git subcommand with no shell. cwd may be empty (inherits current working directory).
// Example: runGit({"diff", "--name-only", "a..b"}, repoRoot)
[[nodiscard]] GitRun runGit(const std::vector<std::string> &args, const std::filesystem::path &cwd);

} // namespace archcheck::git
