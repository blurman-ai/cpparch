#include "archcheck/git/history_query.h"

#include <charconv>
#include <sstream>
#include <utility>

#include "archcheck/git/git_exec.h"

namespace archcheck::git
{

namespace
{

// Parse a line from git log --numstat output: "added\tdeleted\tpath".
// Returns {added, deleted, path} or {-1, -1, ""} on parse failure or rename.
struct ParsedNumStat
{
  std::int32_t added = -1;
  std::int32_t deleted = -1;
  std::string path;
};

// Parses a non-negative count; rejects partial matches and "-" (binary files).
bool parseCount(std::string_view text, std::int32_t &value)
{
  int parsed = 0;
  const char *end = text.data() + text.size();
  const auto [ptr, ec] = std::from_chars(text.data(), end, parsed);
  if (ec != std::errc{} || ptr != end)
    return false;
  value = parsed;
  return true;
}

ParsedNumStat parseNumStatLine(std::string_view line)
{
  if (line.find(" => ") != std::string_view::npos)
    return {};
  const std::size_t tab1 = line.find('\t');
  const std::size_t tab2 = (tab1 == std::string_view::npos) ? tab1 : line.find('\t', tab1 + 1);
  if (tab2 == std::string_view::npos)
    return {};
  ParsedNumStat result;
  if (!parseCount(line.substr(0, tab1), result.added) ||
      !parseCount(line.substr(tab1 + 1, tab2 - tab1 - 1), result.deleted))
    return {};
  result.path = std::string{line.substr(tab2 + 1)};
  return result;
}

} // namespace

std::vector<CommitStats> parseHistoryOutput(std::string_view output)
{
  // Line classes are disjoint, so a single flat pass suffices: a header line
  // ("sha\037subject", separator = ASCII 31) starts a commit, a numstat line
  // attaches to the current commit, blank lines separate blocks.
  std::vector<CommitStats> commits;
  std::istringstream stream{std::string{output}};
  std::string line;
  while (std::getline(stream, line))
  {
    if (line.empty())
      continue;
    const std::size_t sepPos = line.find('\037');
    if (sepPos != std::string::npos)
    {
      commits.push_back({line.substr(0, sepPos), line.substr(sepPos + 1), {}});
      continue;
    }
    if (commits.empty())
      continue;
    const auto stat = parseNumStatLine(line);
    if (stat.added >= 0 && stat.deleted >= 0)
      commits.back().files.push_back({stat.path, stat.added, stat.deleted});
  }
  return commits;
}

std::vector<CommitStats> queryCommitHistory(const std::filesystem::path &repoRoot, std::size_t limit)
{
  // Construct git log command:
  // --numstat: show added/deleted per file
  // --format=%H%x1f%s: commit hash, separator (ASCII 31), subject
  // -n <limit>: limit to last N commits
  // The separator \037 is chosen to be unambiguous in output
  const std::string limitStr = std::to_string(limit);
  const auto result = runGit({"log", "--numstat", "--format=%H%x1f%s", "-n", limitStr}, repoRoot);

  if (result.exitCode != 0)
  {
    return {};
  }

  return parseHistoryOutput(result.out);
}

} // namespace archcheck::git
