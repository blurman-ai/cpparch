#include "archcheck/git/diff_query.h"

#include <cctype>
#include <regex>
#include <sstream>
#include <string_view>

#include "archcheck/git/git_exec.h"
#include "archcheck/git/git_state.h"

namespace archcheck::git
{

namespace
{

// Parse the hunk header from unified diff output, e.g., "@@ -1,5 +3,7 @@" or "@@ -1,0 +2 @@".
// Extracts the starting line number in the new file.
// Returns the line number (1-based), or 0 on parse failure.
int parseHunkNewLineNo(const std::string &line)
{
  // Format: @@ -old_start[,old_count] +new_start[,new_count] @@ [optional text]
  // We want to extract new_start.
  const auto pos = line.find(" +");
  if (pos == std::string::npos)
    return 0;
  const auto commaPos = line.find(',', pos);
  const auto endPos = line.find(' ', pos + 2);
  const auto subEnd = (endPos != std::string::npos) ? endPos : commaPos;
  if (subEnd == std::string::npos)
    return 0;
  try
  {
    return std::stoi(line.substr(pos + 2, subEnd - (pos + 2)));
  }
  catch (...)
  {
    return 0;
  }
}

void processHeaderOrHunk(const std::string &line, std::string &currentFile, int &currentLineNo)
{
  // "diff --git a/path b/path"
  if (line.compare(0, 4, "diff") == 0)
  {
    const auto bPos = line.find(" b/");
    if (bPos != std::string::npos)
      currentFile = line.substr(bPos + 3);
    currentLineNo = 0;
    return;
  }

  // "+++ b/path"
  if (line.compare(0, 3, "+++") == 0)
  {
    if (line.size() > 6 && line.compare(0, 6, "+++ b/") == 0)
      currentFile = line.substr(6);
    currentLineNo = 0;
    return;
  }

  // "@@ -... +... @@"
  if (line.compare(0, 2, "@@") == 0)
    currentLineNo = parseHunkNewLineNo(line);
}

void processContentLine(const std::string &line, const std::string &currentFile, int &currentLineNo,
                        std::vector<AddedLine> &result)
{
  if (line.empty())
    return;

  // Metadata lines. '@' = hunk header: processHeaderOrHunk already set the new
  // line number from it, so it must NOT also be counted as a context line here
  // (that double-count shifted every hunk's added lines by +1).
  if (line[0] == 'i' || line[0] == '-' || line[0] == '\\' || line[0] == '@')
    return;

  // Added line: +content
  if (line[0] == '+')
  {
    if (currentLineNo <= 0 || currentFile.empty())
      return;
    if (line.size() > 3 && line.compare(0, 3, "+++") == 0)
      return; // Skip diff metadata
    result.push_back({currentFile, currentLineNo, line.substr(1)});
    currentLineNo++;
    return;
  }

  // Context line (unified=0 has none, but be safe)
  if (currentLineNo > 0)
    currentLineNo++;
}

// Parse lines from unified diff --unified=0 output.
std::vector<AddedLine> parseUnifiedDiff(std::string_view diffOutput)
{
  std::vector<AddedLine> result;
  std::istringstream iss{std::string{diffOutput}};
  std::string line;
  std::string currentFile;
  int currentLineNo = 0;

  while (std::getline(iss, line))
  {
    processHeaderOrHunk(line, currentFile, currentLineNo);
    processContentLine(line, currentFile, currentLineNo, result);
  }

  return result;
}

} // namespace

std::vector<AddedLine> collectAddedLines(const std::filesystem::path &repoRoot, const std::string &baselineRef,
                                         const std::string &currentRef)
{
  // S6: --no-ext-diff prevents diff.external from running arbitrary programs.
  std::vector<std::string> args{"diff", "--no-ext-diff", "--unified=0"};

  if (currentRef == kWorktreeRef)
  {
    // Comparing ref to working tree
    args.push_back(baselineRef);
  }
  else
  {
    // Comparing two refs
    args.push_back(baselineRef);
    args.push_back(currentRef);
  }

  const auto run = runGit(args, repoRoot);
  if (run.exitCode != 0)
    return {};

  return parseUnifiedDiff(run.out);
}

std::vector<NumStat> parseNumstatOutput(std::string_view output)
{
  std::vector<NumStat> result;
  std::istringstream iss{std::string{output}};
  std::string line;

  while (std::getline(iss, line))
  {
    if (line.empty())
      continue;

    // Format: <added>\t<removed>\t<path>
    const auto tab1 = line.find('\t');
    if (tab1 == std::string::npos)
      continue;
    const auto tab2 = line.find('\t', tab1 + 1);
    if (tab2 == std::string::npos)
      continue;

    try
    {
      const int added = std::stoi(line.substr(0, tab1));
      const int removed = std::stoi(line.substr(tab1 + 1, tab2 - tab1 - 1));
      const auto path = line.substr(tab2 + 1);
      result.push_back({path, added, removed});
    }
    catch (...)
    {
      // Skip unparseable lines
    }
  }

  return result;
}

std::vector<NumStat> collectNumstat(const std::filesystem::path &repoRoot, const std::string &baselineRef,
                                    const std::string &currentRef)
{
  std::vector<std::string> args{"diff", "--no-ext-diff", "--numstat"};
  if (currentRef == kWorktreeRef)
    args.push_back(baselineRef);
  else
  {
    args.push_back(baselineRef);
    args.push_back(currentRef);
  }

  const auto run = runGit(args, repoRoot);
  if (run.exitCode != 0)
    return {};

  return parseNumstatOutput(run.out);
}

} // namespace archcheck::git
