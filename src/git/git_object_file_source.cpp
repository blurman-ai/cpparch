#include "archcheck/git/git_object_file_source.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archcheck/git/git_exec.h"
#include "archcheck/git/git_hardening.h"
#include "archcheck/scan/file_classification.h"

namespace archcheck::git
{

namespace
{

// S4: upper bound on blob size to prevent OOM on adversarial git objects.
constexpr std::size_t kMaxBlobSizeBytes = 64ULL * 1024 * 1024; // 64 MiB

bool pathHasExcludedSegment(const std::string &posixPath)
{
  std::size_t start = 0;
  while (start <= posixPath.size())
  {
    const auto slash = posixPath.find('/', start);
    const auto end = (slash == std::string::npos) ? posixPath.size() : slash;
    if (scan::isExcludedDirName(std::string_view{posixPath}.substr(start, end - start)))
      return true;
    if (slash == std::string::npos)
      break;
    start = slash + 1;
  }
  return false;
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
  // S6: disable system config, hooks, fsmonitor, and pager (shared policy).
  ::setenv("GIT_CONFIG_NOSYSTEM", "1", 1);
  std::vector<char *> argv;
  argv.reserve(kGitHardeningCount + 4);
  argv.push_back(const_cast<char *>("git"));
  for (int i = 0; i < kGitHardeningCount; ++i)
    argv.push_back(const_cast<char *>(kGitHardeningArgs[i]));
  argv.push_back(const_cast<char *>("cat-file"));
  argv.push_back(const_cast<char *>("--batch"));
  argv.push_back(nullptr);
  ::execvp("git", argv.data());
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
  // S6: runGit applies hardening flags and GIT_CONFIG_NOSYSTEM automatically.
  const auto run = runGit({"ls-tree", "-r", "--name-only", ref_}, repoRoot_);
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

// Parse the `git cat-file --batch` header line for a blob object.
// Returns the blob size (which may be 0 for an empty file) on success, or
// std::nullopt if the header is not a blob ("<obj> missing" / non-blob type).
// The distinction matters for stream sync: a blob — even a 0-byte one — is
// followed by <size> content bytes AND a trailing newline that must be consumed;
// a "missing" line has neither. Conflating "empty blob" with "not a blob" left
// the empty blob's trailing newline in the pipe and desynced every later read.
std::optional<std::size_t> GitObjectFileSource::parseBlobSize(const std::string &header)
{
  // Header form: "<sha> <type> <size>" (blob) or "<obj> missing" / "<obj> ambiguous".
  const auto sp1 = header.find(' ');
  if (sp1 == std::string::npos)
    return std::nullopt;
  const auto sp2 = header.find(' ', sp1 + 1);
  if (sp2 == std::string::npos)
    return std::nullopt;
  const std::string_view type = std::string_view{header}.substr(sp1 + 1, sp2 - sp1 - 1);
  if (type != "blob")
    return std::nullopt;
  return static_cast<std::size_t>(std::strtoull(header.data() + sp2 + 1, nullptr, 10));
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
  const auto blobSize = parseBlobSize(header);
  if (!blobSize)
    return {}; // "missing"/non-blob: header line only, no body or trailer to drain
  const std::size_t size = *blobSize;
  if (size > kMaxBlobSizeBytes)
  {
    std::cerr << "archcheck: skipping oversized git blob (> 64 MiB): " << repoRelativePath << '\n';
    std::string discard;
    readExact(discard, size); // drain to keep --batch protocol in sync
    char trailer = 0;
    [[maybe_unused]] auto skip = ::read(stdoutFd_, &trailer, 1);
    return {};
  }
  std::string content;
  if (!readExact(content, size))
    return {};
  char trailer = 0;
  [[maybe_unused]] auto n = ::read(stdoutFd_, &trailer, 1);
  return content;
}

} // namespace archcheck::git
