#include "archcheck/git/git_exec.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "archcheck/git/git_hardening.h"

namespace archcheck::git
{

namespace
{

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
  // S6: set GIT_CONFIG_NOSYSTEM=1 so the system-wide git config is ignored.
  setenv("GIT_CONFIG_NOSYSTEM", "1", 1);

  std::vector<char *> argv;
  argv.reserve(args.size() + kGitHardeningCount + 2);
  argv.push_back(const_cast<char *>("git"));
  for (int i = 0; i < kGitHardeningCount; ++i)
  {
    argv.push_back(const_cast<char *>(kGitHardeningArgs[i]));
  }
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

// Returns forked pid, or -1 on pipe/fork failure (sets r.err).
pid_t tryForkWithPipes(std::array<int, 2> &outPipe, std::array<int, 2> &errPipe, GitRun &r)
{
  if (pipe(outPipe.data()) != 0 || pipe(errPipe.data()) != 0)
  {
    r.err = "pipe() failed"; // LCOV_EXCL_LINE
    return -1;               // LCOV_EXCL_LINE
  }
  const pid_t pid = fork();
  if (pid < 0)
  {
    r.err = "fork() failed"; // LCOV_EXCL_LINE
    return -1;               // LCOV_EXCL_LINE
  }
  return pid;
}

} // namespace

GitRun runGit(const std::vector<std::string> &args, const std::filesystem::path &cwd)
{
  GitRun r;
  std::array<int, 2> outPipe{};
  std::array<int, 2> errPipe{};
  const pid_t pid = tryForkWithPipes(outPipe, errPipe, r);
  if (pid < 0)
    return r;   // LCOV_EXCL_LINE
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

} // namespace archcheck::git
