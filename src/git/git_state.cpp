#include "archcheck/git/git_state.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "archcheck/scan/project_files.h"

namespace archcheck::git
{

namespace
{

struct GitRun
{
  int exitCode = -1;
  std::string out;
  std::string err;
};

// LCOV_EXCL_START — child-process code; runs after fork(), not visible to gcov in parent
[[noreturn]] void execChild(const std::vector<std::string> &args, const std::filesystem::path &cwd, int outFd,
                            int errFd)
{
  dup2(outFd, STDOUT_FILENO);
  dup2(errFd, STDERR_FILENO);
  if (!cwd.empty() && chdir(cwd.c_str()) != 0)
  {
    _exit(127);
  }
  std::vector<char *> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(const_cast<char *>("git"));
  for (const auto &a : args)
  {
    argv.push_back(const_cast<char *>(a.c_str()));
  }
  argv.push_back(nullptr);
  execvp("git", argv.data());
  _exit(127);
}
// LCOV_EXCL_STOP

void drainFd(int fd, std::string &sink)
{
  std::array<char, 4096> buf{};
  for (;;)
  {
    const ssize_t n = read(fd, buf.data(), buf.size());
    if (n <= 0)
      break;
    sink.append(buf.data(), static_cast<std::size_t>(n)); // LCOV_EXCL_BR_LINE
  }
}

void collectChild(int outRead, int errRead, pid_t pid, GitRun &r)
{
  drainFd(outRead, r.out); // LCOV_EXCL_BR_LINE
  drainFd(errRead, r.err); // LCOV_EXCL_BR_LINE
  close(outRead);          // LCOV_EXCL_BR_LINE
  close(errRead);          // LCOV_EXCL_BR_LINE
  int status = 0;
  waitpid(pid, &status, 0); // LCOV_EXCL_BR_LINE
  r.exitCode =
      WIFEXITED(status) ? WEXITSTATUS(status) : -1; // LCOV_EXCL_BR_LINE — false branch requires signal-killed child
}

// fork/exec `git <args>` (no shell). cwd may be empty (== inherit).
GitRun runGit(const std::vector<std::string> &args, const std::filesystem::path &cwd)
{
  GitRun r;
  std::array<int, 2> outPipe{};
  std::array<int, 2> errPipe{};
  if (pipe(outPipe.data()) != 0 || pipe(errPipe.data()) != 0)
  {
    // LCOV_EXCL_START — OS-level failure, not triggerable in unit tests
    r.err = "pipe() failed";
    return r;
    // LCOV_EXCL_STOP
  }
  const pid_t pid = fork();
  if (pid < 0)
  {
    // LCOV_EXCL_START — OS-level failure
    r.err = "fork() failed";
    return r;
    // LCOV_EXCL_STOP
  }
  if (pid == 0) // LCOV_EXCL_BR_LINE — branch taken only in child process
  {
    // LCOV_EXCL_START — child-side of fork; gcov collects data from parent only
    close(outPipe[0]);
    close(errPipe[0]);
    execChild(args, cwd, outPipe[1], errPipe[1]);
    // LCOV_EXCL_STOP
  }
  close(outPipe[1]);                            // LCOV_EXCL_BR_LINE
  close(errPipe[1]);                            // LCOV_EXCL_BR_LINE
  collectChild(outPipe[0], errPipe[0], pid, r); // LCOV_EXCL_BR_LINE
  return r;
}

std::string trimTrailingNewline(std::string s)
{
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
  {
    s.pop_back();
  }
  return s;
}

std::filesystem::path mkTempWorktreeDir()
{
  const auto base = std::filesystem::temp_directory_path() / "archcheck-worktree-XXXXXX";
  std::string templ = base.string();
  // mkdtemp mutates the template in place; it must be a writable C string.
  std::vector<char> buf(templ.begin(), templ.end());
  buf.push_back('\0');
  if (mkdtemp(buf.data()) == nullptr)
  {
    return {}; // LCOV_EXCL_LINE — mkdtemp failure not reproducible in unit tests
  }
  return std::filesystem::path{buf.data()};
}

} // namespace

std::optional<Revspec> parseRevspec(std::string_view input)
{
  if (input.empty())
  {
    return std::nullopt;
  }
  const auto pos = input.find("..");
  if (pos == std::string_view::npos)
  {
    return Revspec{std::string{input}, std::string{kWorktreeRef}};
  }
  // Reject "...", and empty sides.
  if (pos + 2 < input.size() && input[pos + 2] == '.')
    return std::nullopt;
  const auto lhs = input.substr(0, pos);
  const auto rhs = input.substr(pos + 2);
  if (lhs.empty() || rhs.empty())
    return std::nullopt;
  return Revspec{std::string{lhs}, std::string{rhs}};
}

std::optional<std::filesystem::path> findRepoRoot(const std::filesystem::path &inside)
{
  const auto run = runGit({"rev-parse", "--show-toplevel"}, inside);
  if (run.exitCode != 0)
  {
    return std::nullopt;
  }
  const auto trimmed = trimTrailingNewline(run.out);
  if (trimmed.empty())
  {
    return std::nullopt;
  }
  return std::filesystem::path{trimmed};
}

Worktree::Worktree(std::filesystem::path repoRoot, std::filesystem::path workPath)
    : repo_(std::move(repoRoot)), work_(std::move(workPath))
{
}

Worktree::Worktree(Worktree &&other) noexcept : repo_(std::move(other.repo_)), work_(std::move(other.work_))
{
  other.work_.clear();
  other.repo_.clear();
}

Worktree &Worktree::operator=(Worktree &&other) noexcept
{
  if (this != &other)
  {
    release();
    repo_ = std::move(other.repo_);
    work_ = std::move(other.work_);
    other.work_.clear();
    other.repo_.clear();
  }
  return *this;
}

Worktree::~Worktree() { release(); }

void Worktree::release()
{
  if (work_.empty() || repo_.empty())
  {
    return;
  }
  // Best-effort: ask git first, then nuke directory if it still exists.
  runGit({"worktree", "remove", "--force", work_.string()}, repo_);
  std::error_code ec;
  std::filesystem::remove_all(work_, ec);
  work_.clear();
  repo_.clear();
}

namespace
{

void splitLines(const std::string &s, std::vector<std::string> &out)
{
  std::string line;
  std::istringstream iss(s);
  while (std::getline(iss, line))
  {
    if (!line.empty())
      out.push_back(std::move(line));
    line.clear();
  }
}

} // namespace

namespace
{

bool collectChangedVsWorktree(const std::filesystem::path &repoRoot, const std::string &baselineRef,
                              std::vector<std::string> &out)
{
  const auto diff = runGit({"diff", "--name-only", "--diff-filter=ACMRD", baselineRef}, repoRoot);
  if (diff.exitCode != 0)
    return false;
  splitLines(diff.out, out);
  // Plus untracked files (e.g. brand-new `c.h` not yet `git add`-ed).
  const auto untracked = runGit({"ls-files", "--others", "--exclude-standard"}, repoRoot);
  if (untracked.exitCode != 0)
    return false;
  splitLines(untracked.out, out);
  return true;
}

bool collectChangedTwoRefs(const std::filesystem::path &repoRoot, const std::string &a, const std::string &b,
                           std::vector<std::string> &out)
{
  const auto diff = runGit({"diff", "--name-only", "--diff-filter=ACMRD", a, b}, repoRoot);
  if (diff.exitCode != 0)
    return false;
  splitLines(diff.out, out);
  return true;
}

} // namespace

std::optional<std::vector<std::filesystem::path>>
changedCppFiles(const std::filesystem::path &repoRoot, const std::string &baselineRef, const std::string &currentRef)
{
  std::vector<std::string> raw;
  const bool ok = (currentRef == kWorktreeRef) ? collectChangedVsWorktree(repoRoot, baselineRef, raw)
                                               : collectChangedTwoRefs(repoRoot, baselineRef, currentRef, raw);
  if (!ok)
    return std::nullopt; // LCOV_EXCL_LINE — git failure not triggered in unit tests

  std::vector<std::filesystem::path> result;
  result.reserve(raw.size());
  for (auto &p : raw)
  {
    std::filesystem::path path{p};
    if (scan::hasProjectExtension(path))
      result.push_back(std::move(path));
  }
  return result;
}

std::optional<Worktree> materializeRef(const std::filesystem::path &repoRoot, const std::string &ref, GitError &err)
{
  if (ref == kWorktreeRef)
  {
    // Non-owning: empty repo_ disables cleanup.
    Worktree wt;
    wt = Worktree{std::filesystem::path{}, repoRoot};
    return wt;
  }
  const auto tmp = mkTempWorktreeDir();
  if (tmp.empty())
  {
    // LCOV_EXCL_START — depends on mkdtemp failure
    err.message = "failed to create temp worktree directory";
    return std::nullopt;
    // LCOV_EXCL_STOP
  }
  // `git worktree add` refuses a non-empty directory; mkdtemp gives us an
  // empty one but git wants to create it itself, so remove first.
  std::error_code ec;
  std::filesystem::remove(tmp, ec);

  const auto run = runGit({"worktree", "add", "--detach", "--quiet", tmp.string(), ref}, repoRoot);
  if (run.exitCode != 0)
  {
    err.message = "git worktree add failed: " + trimTrailingNewline(run.err);
    return std::nullopt;
  }
  return Worktree{repoRoot, tmp};
}

} // namespace archcheck::git
