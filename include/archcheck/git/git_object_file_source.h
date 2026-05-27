#pragma once

#include <sys/types.h>

#include <filesystem>
#include <string>
#include <vector>

#include "archcheck/scan/file_source.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::git
{

// FileSource backed by a long-lived `git cat-file --batch` subprocess and
// `git ls-tree -r <ref>` for enumeration. Lets `--diff` read project files
// directly from git's object database without materialising a worktree.
// Owns one child process from construction to destruction (RAII).
class GitObjectFileSource final : public scan::FileSource
{
public:
  GitObjectFileSource(std::filesystem::path repoRoot, std::string ref);
  ~GitObjectFileSource() override;

  GitObjectFileSource(const GitObjectFileSource &) = delete;
  GitObjectFileSource &operator=(const GitObjectFileSource &) = delete;
  GitObjectFileSource(GitObjectFileSource &&) = delete;
  GitObjectFileSource &operator=(GitObjectFileSource &&) = delete;

  std::vector<scan::ProjectFile> list() override;
  std::string read(const std::string &repoRelativePath) override;

  // True iff the `cat-file --batch` child was successfully spawned.
  bool valid() const { return pid_ > 0; }

private:
  bool spawnCatFileBatch();
  void closeChild();
  bool readLine(std::string &line);
  bool readExact(std::string &out, std::size_t n);

  std::filesystem::path repoRoot_;
  std::string ref_;
  pid_t pid_ = -1;
  int stdinFd_ = -1;
  int stdoutFd_ = -1;
};

} // namespace archcheck::git
