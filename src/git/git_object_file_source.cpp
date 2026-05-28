#include "archcheck/git/git_object_file_source.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace archcheck::git
{

namespace
{

constexpr std::array<std::string_view, 6> kExcludedSegments = {
    ".git", "build", ".cache", ".idea", ".vscode", "out",
};
constexpr std::string_view kCmakeBuildPrefix = "cmake-build-";

bool isExcludedSegment(std::string_view s)
{
  if (std::find(kExcludedSegments.begin(), kExcludedSegments.end(), s) != kExcludedSegments.end())
    return true;
  return s.size() >= kCmakeBuildPrefix.size() && s.compare(0, kCmakeBuildPrefix.size(), kCmakeBuildPrefix) == 0;
}

bool pathHasExcludedSegment(const std::string &posixPath)
{
  std::size_t start = 0;
  while (start <= posixPath.size())
  {
    const auto slash = posixPath.find('/', start);
    const auto end = (slash == std::string::npos) ? posixPath.size() : slash;
    if (isExcludedSegment(std::string_view{posixPath}.substr(start, end - start)))
      return true;
    if (slash == std::string::npos)
      break;
    start = slash + 1;
  }
  return false;
}

// One-shot helper for `git ls-tree`. Mirrors the fork/exec pattern in
// git_state.cpp but is intentionally local: GitObjectFileSource is the
// only caller and we want to keep its translation unit self-contained.
struct OneShot
{
  int exitCode = -1;
  std::string out;
};

void drain(int fd, std::string &sink)
{
  std::array<char, 4096> buf{};
  for (;;)
  {
    const ssize_t n = ::read(fd, buf.data(), buf.size());
    if (n <= 0)
      break;
    sink.append(buf.data(), static_cast<std::size_t>(n)); // LCOV_EXCL_BR_LINE
  }
}

// LCOV_EXCL_START — child-process code; runs after fork(), not visible to gcov in parent
[[noreturn]] void execGitChild(const std::vector<std::string> &args, const std::filesystem::path &cwd, int outFd)
{
  ::dup2(outFd, STDOUT_FILENO);
  if (!cwd.empty() && ::chdir(cwd.c_str()) != 0)
    _exit(127);
  std::vector<char *> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(const_cast<char *>("git"));
  for (const auto &a : args)
    argv.push_back(const_cast<char *>(a.c_str()));
  argv.push_back(nullptr);
  ::execvp("git", argv.data());
  _exit(127);
}
// LCOV_EXCL_STOP

OneShot runGitOneShot(const std::vector<std::string> &args, const std::filesystem::path &cwd)
{
  OneShot r;
  std::array<int, 2> outPipe{};
  if (::pipe(outPipe.data()) != 0)
    return r; // LCOV_EXCL_LINE — OS-level failure
  const pid_t pid = ::fork();
  if (pid < 0)
  {
    // LCOV_EXCL_START — OS-level failure
    ::close(outPipe[0]);
    ::close(outPipe[1]);
    return r;
    // LCOV_EXCL_STOP
  }
  if (pid == 0) // LCOV_EXCL_BR_LINE — branch taken only in child process
  {
    // LCOV_EXCL_START — child-side of fork
    ::close(outPipe[0]);
    execGitChild(args, cwd, outPipe[1]);
    // LCOV_EXCL_STOP
  }
  ::close(outPipe[1]);
  drain(outPipe[0], r.out);
  ::close(outPipe[0]);
  int status = 0;
  ::waitpid(pid, &status, 0);
  r.exitCode =
      WIFEXITED(status) ? WEXITSTATUS(status) : -1; // LCOV_EXCL_BR_LINE — false branch requires signal-killed child
  return r;
}

} // namespace

GitObjectFileSource::GitObjectFileSource(std::filesystem::path repoRoot, std::string ref)
    : repoRoot_(std::move(repoRoot)), ref_(std::move(ref))
{
  spawnCatFileBatch();
}

GitObjectFileSource::~GitObjectFileSource() { closeChild(); }

namespace
{

// LCOV_EXCL_START — child-process code after fork()
[[noreturn]] void execCatFileBatch(const std::filesystem::path &cwd, int childStdin, int childStdout)
{
  ::dup2(childStdin, STDIN_FILENO);
  ::dup2(childStdout, STDOUT_FILENO);
  if (!cwd.empty() && ::chdir(cwd.c_str()) != 0)
    _exit(127);
  char *const argv[] = {const_cast<char *>("git"), const_cast<char *>("cat-file"), const_cast<char *>("--batch"),
                        nullptr};
  ::execvp("git", argv);
  _exit(127);
}
// LCOV_EXCL_STOP

} // namespace

bool GitObjectFileSource::spawnCatFileBatch()
{
  std::array<int, 2> inPipe{};
  std::array<int, 2> outPipe{};
  if (::pipe(inPipe.data()) != 0 || ::pipe(outPipe.data()) != 0)
    return false; // LCOV_EXCL_LINE — OS-level failure
  const pid_t pid = ::fork();
  if (pid < 0)
    return false; // LCOV_EXCL_LINE — OS-level failure
  if (pid == 0)   // LCOV_EXCL_BR_LINE — branch taken only in child process
  {
    // LCOV_EXCL_START — child-side of fork
    ::close(inPipe[1]);
    ::close(outPipe[0]);
    execCatFileBatch(repoRoot_, inPipe[0], outPipe[1]);
    // LCOV_EXCL_STOP
  }
  ::close(inPipe[0]);
  ::close(outPipe[1]);
  pid_ = pid;
  stdinFd_ = inPipe[1];
  stdoutFd_ = outPipe[0];
  return true;
}

void GitObjectFileSource::closeChild()
{
  if (stdinFd_ >= 0)
  {
    ::close(stdinFd_);
    stdinFd_ = -1;
  }
  if (stdoutFd_ >= 0)
  {
    ::close(stdoutFd_);
    stdoutFd_ = -1;
  }
  if (pid_ > 0)
  {
    int status = 0;
    ::waitpid(pid_, &status, 0);
    pid_ = -1;
  }
}

std::vector<scan::ProjectFile> GitObjectFileSource::list()
{
  std::vector<scan::ProjectFile> out;
  const auto run = runGitOneShot({"ls-tree", "-r", "--name-only", ref_}, repoRoot_);
  if (run.exitCode != 0)
    return out;
  std::istringstream iss(run.out);
  std::string line;
  while (std::getline(iss, line))
  {
    if (line.empty())
      continue;
    if (!scan::hasProjectExtension(std::filesystem::path{line}))
      continue;
    if (pathHasExcludedSegment(line))
      continue;
    out.push_back(scan::ProjectFile{line});
  }
  std::sort(out.begin(), out.end(),
            [](const scan::ProjectFile &a, const scan::ProjectFile &b) { return a.path < b.path; });
  return out;
}

bool GitObjectFileSource::readLine(std::string &line)
{
  line.clear();
  char c = 0;
  for (;;)
  {
    const ssize_t n = ::read(stdoutFd_, &c, 1);
    if (n <= 0)
      return false;
    if (c == '\n')
      return true;
    line.push_back(c);
  }
}

bool GitObjectFileSource::readExact(std::string &out, std::size_t n)
{
  out.resize(n);
  std::size_t got = 0;
  while (got < n)
  {
    const ssize_t k = ::read(stdoutFd_, out.data() + got, n - got);
    if (k <= 0)
      return false;
    got += static_cast<std::size_t>(k);
  }
  return true;
}

std::string GitObjectFileSource::read(const std::string &repoRelativePath)
{
  if (pid_ <= 0)
    return {};
  const std::string req = ref_ + ":" + repoRelativePath + "\n";
  if (::write(stdinFd_, req.data(), req.size()) != static_cast<ssize_t>(req.size()))
    return {};
  std::string header;
  if (!readLine(header))
    return {};
  // Header form: "<sha> <type> <size>" (blob) or "<obj> missing" / "<obj> ambiguous".
  const auto sp1 = header.find(' ');
  if (sp1 == std::string::npos)
    return {};
  const auto sp2 = header.find(' ', sp1 + 1);
  if (sp2 == std::string::npos)
    return {};
  const std::string_view type = std::string_view{header}.substr(sp1 + 1, sp2 - sp1 - 1);
  if (type != "blob")
    return {};
  const std::size_t size = static_cast<std::size_t>(std::strtoull(header.data() + sp2 + 1, nullptr, 10));
  std::string content;
  if (!readExact(content, size))
    return {};
  char trailer = 0;
  (void)::read(stdoutFd_, &trailer, 1);
  return content;
}

} // namespace archcheck::git
