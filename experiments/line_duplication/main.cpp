#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "archcheck/git/git_object_file_source.h"
#include "archcheck/git/git_state.h"

namespace fs = std::filesystem;

namespace
{

struct GitMode
{
  std::string baselineRef;
  std::string currentRef;
  fs::path repoRoot;
  std::string scopePrefix;
  std::unordered_set<std::string> changedFiles;
};

struct Options
{
  fs::path root;
  std::size_t minLines = 5;
  std::size_t minWindowChars = 60;
  std::size_t top = 8;
  bool crossOnly = false;
  std::optional<std::string> gitDiff;
  std::optional<std::string> gitCommit;
  std::vector<std::string> excludePatterns = {
    "third_party",
    "bundled",
    "generated",
    "*_generated*",
    "*.pb.*",
  };
};

struct SigLine
{
  std::string text;
  std::uint32_t line = 0;
};

struct FileData
{
  std::string displayPath;
  std::string sourcePath;
  std::vector<SigLine> lines;
  std::vector<std::uint8_t> covered;
};

struct InputCorpus
{
  std::vector<FileData> files;
  std::size_t discoveredFiles = 0;
};

struct WindowPos
{
  std::uint32_t fileIndex = 0;
  std::uint32_t sigIndex = 0;
};

struct SeedPair
{
  WindowPos a;
  WindowPos b;
};

struct Block
{
  std::string fileA;
  std::uint32_t startA = 0;
  std::string fileB;
  std::uint32_t startB = 0;
  std::uint32_t length = 0;
  std::uint32_t fileIndexA = 0;
  std::uint32_t fileIndexB = 0;
  std::uint32_t sigIndexA = 0;
  std::uint32_t sigIndexB = 0;
  std::uint64_t fingerprint = 0;
  bool touchesChanged = false;
  bool introduced = false;
};

struct Result
{
  std::size_t discoveredFiles = 0;
  std::size_t eligibleFiles = 0;
  std::size_t totalSigLines = 0;
  std::size_t coveredSigLines = 0;
  double duplicationRatio = 0.0;
  std::vector<Block> blocks;
};

struct CommitDiffReport
{
  Result baseline;
  Result current;
  std::vector<Block> introducedBlocks;
};

struct RegionKey
{
  std::uint32_t fileA = 0;
  std::uint32_t fileB = 0;
  std::int64_t diagonal = 0;

  bool operator==(const RegionKey &other) const noexcept
  {
    return fileA == other.fileA && fileB == other.fileB && diagonal == other.diagonal;
  }
};

struct RegionKeyHash
{
  std::size_t operator()(const RegionKey &key) const noexcept
  {
    std::size_t hash = static_cast<std::size_t>(key.fileA);
    hash ^= static_cast<std::size_t>(key.fileB) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= static_cast<std::size_t>(key.diagonal) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    return hash;
  }
};

struct StringViewHash
{
  std::size_t operator()(std::string_view value) const noexcept
  {
    return std::hash<std::string_view>{}(value);
  }
};

struct BlockKey
{
  std::string fileA;
  std::string fileB;
  std::uint64_t fingerprint = 0;

  bool operator==(const BlockKey &other) const noexcept
  {
    return fileA == other.fileA && fileB == other.fileB && fingerprint == other.fingerprint;
  }
};

struct BlockKeyHash
{
  std::size_t operator()(const BlockKey &key) const noexcept
  {
    std::size_t hash = std::hash<std::string>{}(key.fileA);
    hash ^= std::hash<std::string>{}(key.fileB) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= static_cast<std::size_t>(key.fingerprint) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    return hash;
  }
};

[[nodiscard]] std::string trim(std::string_view value)
{
  std::size_t first = 0;
  while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
    ++first;

  std::size_t last = value.size();
  while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
    --last;

  return std::string(value.substr(first, last - first));
}

[[nodiscard]] std::string collapseWhitespace(std::string_view value)
{
  std::string out;
  out.reserve(value.size());

  bool inSpace = false;
  for (char ch : value)
  {
    if (std::isspace(static_cast<unsigned char>(ch)))
    {
      if (!out.empty())
        inSpace = true;
      continue;
    }

    if (inSpace)
    {
      out.push_back(' ');
      inSpace = false;
    }

    out.push_back(ch);
  }

  return out;
}

[[nodiscard]] std::string normalizeLine(std::string_view raw)
{
  return collapseWhitespace(trim(raw));
}

[[nodiscard]] bool startsWith(std::string_view value, std::string_view prefix)
{
  return value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool isCommentOnly(std::string_view raw)
{
  const std::string trimmed = trim(raw);
  return startsWith(trimmed, "//") || startsWith(trimmed, "/*") || startsWith(trimmed, "*") ||
         startsWith(trimmed, "*/");
}

[[nodiscard]] bool isPunctuationOnly(std::string_view value)
{
  if (value.empty())
    return false;

  for (char ch : value)
  {
    if (ch == ' ')
      continue;
    if (ch != '{' && ch != '}' && ch != '(' && ch != ')' && ch != ';')
      return false;
  }

  return true;
}

[[nodiscard]] bool isNamespaceLine(std::string_view value)
{
  if (!startsWith(value, "namespace"))
    return false;

  if (value.size() == std::string_view("namespace").size())
    return true;

  const char next = value[std::string_view("namespace").size()];
  return std::isspace(static_cast<unsigned char>(next)) != 0;
}

[[nodiscard]] bool isTrivialLine(std::string_view value)
{
  static const std::unordered_set<std::string_view, StringViewHash> kKeywords = {
    "else",
    "break;",
    "continue;",
    "return;",
    "#endif",
    "#pragma once",
    "public:",
    "private:",
    "protected:",
    "}",
    "};",
  };

  return isPunctuationOnly(value) || isNamespaceLine(value) || kKeywords.find(value) != kKeywords.end();
}

[[nodiscard]] bool hasWildcard(std::string_view value)
{
  return value.find('*') != std::string_view::npos || value.find('?') != std::string_view::npos;
}

[[nodiscard]] std::vector<std::string> splitPathString(const std::string &path)
{
  std::vector<std::string> out;
  std::string current;

  for (char ch : path)
  {
    if (ch == '/')
    {
      if (!current.empty())
        out.push_back(std::exchange(current, ""));
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    out.push_back(current);

  return out;
}

[[nodiscard]] bool globMatch(std::string_view pattern, std::string_view text)
{
  std::size_t p = 0;
  std::size_t t = 0;
  std::size_t star = std::string_view::npos;
  std::size_t starText = 0;

  while (t < text.size())
  {
    if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t]))
    {
      ++p;
      ++t;
      continue;
    }

    if (p < pattern.size() && pattern[p] == '*')
    {
      star = p++;
      starText = t;
      continue;
    }

    if (star == std::string_view::npos)
      return false;

    p = star + 1;
    t = ++starText;
  }

  while (p < pattern.size() && pattern[p] == '*')
    ++p;

  return p == pattern.size();
}

class ExcludeMatcher
{
public:
  explicit ExcludeMatcher(std::vector<std::string> patterns) : patterns_(std::move(patterns)) {}

  [[nodiscard]] bool matches(const std::string &relativePath) const
  {
    const std::vector<std::string> parts = splitPathString(relativePath);
    const std::string fileName = parts.empty() ? relativePath : parts.back();

    for (const std::string &pattern : patterns_)
    {
      if (hasWildcard(pattern))
      {
        if (globMatch(pattern, relativePath) || globMatch(pattern, fileName))
          return true;
        continue;
      }

      if (relativePath == pattern)
        return true;

      if (std::find(parts.begin(), parts.end(), pattern) != parts.end())
        return true;
    }

    return false;
  }

private:
  std::vector<std::string> patterns_;
};

[[nodiscard]] bool isSkippedDirectoryName(std::string_view name)
{
  return name == ".git" || name == ".cache" || name == ".idea" || name == ".vscode" || name == "build" ||
         name == "out" || startsWith(name, "cmake-build-");
}

[[nodiscard]] bool hasSkippedDirectoryComponent(const std::string &relativePath)
{
  for (const std::string &part : splitPathString(relativePath))
  {
    if (isSkippedDirectoryName(part))
      return true;
  }

  return false;
}

[[nodiscard]] bool hasProjectExtension(const fs::path &path)
{
  const std::string ext = path.extension().string();
  return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".h" || ext == ".hh" ||
         ext == ".hpp" || ext == ".hxx" || ext == ".ipp" || ext == ".tpp" || ext == ".inl" || ext == ".inc";
}

[[nodiscard]] std::vector<fs::path> discoverSources(const fs::path &root, const ExcludeMatcher &matcher)
{
  std::vector<fs::path> files;
  fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied);

  for (const fs::directory_entry &entry : it)
  {
    const fs::path rel = fs::relative(entry.path(), root);
    const std::string relString = rel.generic_string();

    if (entry.is_directory())
    {
      if (isSkippedDirectoryName(entry.path().filename().string()) || hasSkippedDirectoryComponent(relString) ||
          matcher.matches(relString))
      {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!entry.is_regular_file() || hasSkippedDirectoryComponent(relString) || !hasProjectExtension(entry.path()) ||
        matcher.matches(relString))
    {
      continue;
    }

    files.push_back(rel);
  }

  std::sort(files.begin(), files.end());
  return files;
}

[[nodiscard]] std::vector<SigLine> readSignificantLines(std::string_view content)
{
  std::vector<SigLine> lines;
  std::string raw;
  std::uint32_t lineNo = 0;

  std::size_t start = 0;
  while (start <= content.size())
  {
    const std::size_t end = content.find('\n', start);
    const std::size_t count = (end == std::string_view::npos) ? content.size() - start : end - start;
    raw.assign(content.substr(start, count));
    if (!raw.empty() && raw.back() == '\r')
      raw.pop_back();

    ++lineNo;
    if (!trim(raw).empty() && !isCommentOnly(raw))
      lines.push_back({normalizeLine(raw), lineNo});

    if (end == std::string_view::npos)
      break;
    start = end + 1;
  }

  return lines;
}

[[nodiscard]] std::vector<SigLine> readSignificantLinesFromFile(const fs::path &path)
{
  std::ifstream input(path);
  if (!input)
    return {};

  std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return readSignificantLines(content);
}

[[nodiscard]] bool isTrivialWindow(const std::vector<SigLine> &lines,
                                   std::size_t start,
                                   std::size_t minLines,
                                   std::size_t minWindowChars)
{
  bool allTrivial = true;
  std::size_t totalChars = 0;

  for (std::size_t index = start; index < start + minLines; ++index)
  {
    const std::string &text = lines[index].text;
    totalChars += text.size();
    allTrivial = allTrivial && isTrivialLine(text);
  }

  return allTrivial || totalChars < minWindowChars;
}

[[nodiscard]] std::uint64_t fnv1aAppend(std::uint64_t hash, std::string_view text)
{
  for (char ch : text)
  {
    hash ^= static_cast<unsigned char>(ch);
    hash *= 1099511628211ULL;
  }

  return hash;
}

[[nodiscard]] std::uint64_t hashWindow(const std::vector<SigLine> &lines, std::size_t start, std::size_t minLines)
{
  std::uint64_t hash = 1469598103934665603ULL;

  for (std::size_t index = start; index < start + minLines; ++index)
  {
    hash = fnv1aAppend(hash, lines[index].text);
    hash = fnv1aAppend(hash, "\n");
  }

  return hash;
}

[[nodiscard]] std::string windowKey(const std::vector<SigLine> &lines, std::size_t start, std::size_t minLines)
{
  std::string key;
  for (std::size_t index = start; index < start + minLines; ++index)
  {
    key += lines[index].text;
    key.push_back('\n');
  }
  return key;
}

void markCovered(FileData &file, std::size_t start, std::size_t minLines)
{
  for (std::size_t index = start; index < start + minLines; ++index)
    file.covered[index] = 1;
}

[[nodiscard]] SeedPair chooseSeed(const std::vector<WindowPos> &positions, const std::vector<FileData> &files)
{
  const WindowPos first = positions.front();
  for (std::size_t index = 1; index < positions.size(); ++index)
  {
    if (files[positions[index].fileIndex].displayPath != files[first.fileIndex].displayPath)
      return {first, positions[index]};
  }

  return {first, positions[1]};
}

[[nodiscard]] std::uint64_t fingerprintBlockContent(const std::vector<FileData> &files,
                                                    std::uint32_t fileIndex,
                                                    std::uint32_t sigIndex,
                                                    std::uint32_t length)
{
  std::uint64_t hash = 1469598103934665603ULL;
  hash = fnv1aAppend(hash, std::to_string(length));
  hash = fnv1aAppend(hash, "\n");

  const auto &lines = files[fileIndex].lines;
  for (std::size_t index = sigIndex; index < sigIndex + length; ++index)
  {
    hash = fnv1aAppend(hash, lines[index].text);
    hash = fnv1aAppend(hash, "\n");
  }

  return hash;
}

[[nodiscard]] std::vector<Block> maximalBlocks(const std::vector<SeedPair> &seeds,
                                               const std::vector<FileData> &files,
                                               std::size_t minLines)
{
  struct Extended
  {
    std::uint32_t fileA = 0;
    std::uint32_t fileB = 0;
    std::uint32_t startA = 0;
    std::uint32_t startB = 0;
    std::uint32_t length = 0;
  };

  std::vector<Extended> extended;
  extended.reserve(seeds.size());

  for (const SeedPair &seed : seeds)
  {
    const std::vector<SigLine> &a = files[seed.a.fileIndex].lines;
    const std::vector<SigLine> &b = files[seed.b.fileIndex].lines;

    std::uint32_t length = static_cast<std::uint32_t>(minLines);
    while (seed.a.sigIndex + length < a.size() && seed.b.sigIndex + length < b.size() &&
           a[seed.a.sigIndex + length].text == b[seed.b.sigIndex + length].text)
    {
      ++length;
    }

    extended.push_back({seed.a.fileIndex, seed.b.fileIndex, seed.a.sigIndex, seed.b.sigIndex, length});
  }

  std::sort(extended.begin(), extended.end(), [](const Extended &lhs, const Extended &rhs) {
    if (lhs.length != rhs.length)
      return lhs.length > rhs.length;
    if (lhs.fileA != rhs.fileA)
      return lhs.fileA < rhs.fileA;
    if (lhs.fileB != rhs.fileB)
      return lhs.fileB < rhs.fileB;
    return lhs.startA < rhs.startA;
  });

  std::unordered_map<RegionKey, std::vector<std::pair<std::uint32_t, std::uint32_t>>, RegionKeyHash> emitted;
  std::vector<Block> blocks;

  for (const Extended &item : extended)
  {
    const RegionKey region = {item.fileA, item.fileB, static_cast<std::int64_t>(item.startA) - item.startB};
    const auto &intervals = emitted[region];

    bool covered = false;
    for (const auto &interval : intervals)
    {
      if (interval.first <= item.startA && item.startA < interval.second)
      {
        covered = true;
        break;
      }
    }

    if (covered)
      continue;

    emitted[region].push_back({item.startA, static_cast<std::uint32_t>(item.startA + item.length)});

    Block block;
    block.fileA = files[item.fileA].displayPath;
    block.startA = files[item.fileA].lines[item.startA].line;
    block.fileB = files[item.fileB].displayPath;
    block.startB = files[item.fileB].lines[item.startB].line;
    block.length = item.length;
    block.fileIndexA = item.fileA;
    block.fileIndexB = item.fileB;
    block.sigIndexA = item.startA;
    block.sigIndexB = item.startB;
    block.fingerprint = fingerprintBlockContent(files, item.fileA, item.startA, item.length);
    blocks.push_back(std::move(block));
  }

  std::sort(blocks.begin(), blocks.end(), [](const Block &lhs, const Block &rhs) {
    if (lhs.length != rhs.length)
      return lhs.length > rhs.length;
    if (lhs.fileA != rhs.fileA)
      return lhs.fileA < rhs.fileA;
    if (lhs.fileB != rhs.fileB)
      return lhs.fileB < rhs.fileB;
    return lhs.startA < rhs.startA;
  });

  return blocks;
}

[[nodiscard]] Result detect(InputCorpus corpus, const Options &options)
{
  std::unordered_map<std::uint64_t, std::vector<WindowPos>> buckets;

  for (std::uint32_t fileIndex = 0; fileIndex < corpus.files.size(); ++fileIndex)
  {
    const auto &lines = corpus.files[fileIndex].lines;
    for (std::size_t start = 0; start + options.minLines <= lines.size(); ++start)
    {
      if (isTrivialWindow(lines, start, options.minLines, options.minWindowChars))
        continue;

      const std::uint64_t hash = hashWindow(lines, start, options.minLines);
      buckets[hash].push_back({fileIndex, static_cast<std::uint32_t>(start)});
    }
  }

  std::vector<SeedPair> seeds;
  for (const auto &entry : buckets)
  {
    (void)entry.first;
    const auto &positions = entry.second;
    if (positions.size() < 2)
      continue;

    std::unordered_map<std::string, std::vector<WindowPos>> exactGroups;
    for (const WindowPos &pos : positions)
      exactGroups[windowKey(corpus.files[pos.fileIndex].lines, pos.sigIndex, options.minLines)].push_back(pos);

    for (const auto &groupEntry : exactGroups)
    {
      const auto &group = groupEntry.second;
      if (group.size() < 2)
        continue;

      for (const WindowPos &pos : group)
        markCovered(corpus.files[pos.fileIndex], pos.sigIndex, options.minLines);

      seeds.push_back(chooseSeed(group, corpus.files));
    }
  }

  Result result;
  result.discoveredFiles = corpus.discoveredFiles;
  result.eligibleFiles = corpus.files.size();

  for (const FileData &file : corpus.files)
  {
    result.totalSigLines += file.lines.size();
    result.coveredSigLines += std::count(file.covered.begin(), file.covered.end(), static_cast<std::uint8_t>(1));
  }

  if (result.totalSigLines != 0)
  {
    result.duplicationRatio = static_cast<double>(result.coveredSigLines) / static_cast<double>(result.totalSigLines);
  }

  result.blocks = maximalBlocks(seeds, corpus.files, options.minLines);
  return result;
}

[[nodiscard]] bool isCrossFile(const Block &block)
{
  return block.fileA != block.fileB;
}

[[nodiscard]] bool shouldShowBlock(const Block &block, const Options &options)
{
  return !options.crossOnly || isCrossFile(block);
}

[[nodiscard]] bool pathInScope(const std::string &repoRelativePath, const std::string &scopePrefix)
{
  if (scopePrefix.empty())
    return true;

  if (repoRelativePath == scopePrefix)
    return true;

  const std::string prefix = scopePrefix + "/";
  return repoRelativePath.size() > prefix.size() && startsWith(repoRelativePath, prefix);
}

[[nodiscard]] std::string makeDisplayPath(const std::string &repoRelativePath, const std::string &scopePrefix)
{
  if (scopePrefix.empty())
    return repoRelativePath;

  const std::string prefix = scopePrefix + "/";
  if (startsWith(repoRelativePath, prefix))
    return repoRelativePath.substr(prefix.size());

  return repoRelativePath;
}

[[nodiscard]] std::string computeScopePrefix(const fs::path &scanRoot, const fs::path &repoRoot)
{
  std::error_code ec;
  const fs::path relative = fs::relative(scanRoot, repoRoot, ec);
  if (ec)
    return {};

  const std::string value = relative.generic_string();
  return (value == "." || value.empty()) ? std::string{} : value;
}

[[nodiscard]] InputCorpus loadFilesystemCorpus(const Options &options)
{
  const ExcludeMatcher matcher(options.excludePatterns);
  const std::vector<fs::path> discovered = discoverSources(options.root, matcher);

  InputCorpus corpus;
  corpus.discoveredFiles = discovered.size();

  for (const fs::path &relative : discovered)
  {
    FileData file;
    file.displayPath = relative.generic_string();
    file.sourcePath = file.displayPath;
    file.lines = readSignificantLinesFromFile(options.root / relative);

    if (file.lines.size() < options.minLines)
      continue;

    file.covered.assign(file.lines.size(), 0);
    corpus.files.push_back(std::move(file));
  }

  return corpus;
}

[[nodiscard]] InputCorpus loadGitCorpus(const Options &options,
                                        const fs::path &repoRoot,
                                        const std::string &scopePrefix,
                                        const std::string &ref)
{
  const ExcludeMatcher matcher(options.excludePatterns);
  archcheck::git::GitObjectFileSource source(repoRoot, ref);
  if (!source.valid())
    throw std::runtime_error("failed to spawn git cat-file --batch");

  const auto listed = source.list();
  InputCorpus corpus;

  for (const auto &projectFile : listed)
  {
    if (!pathInScope(projectFile.path, scopePrefix))
      continue;

    const std::string displayPath = makeDisplayPath(projectFile.path, scopePrefix);
    if (matcher.matches(displayPath))
      continue;

    ++corpus.discoveredFiles;

    FileData file;
    file.displayPath = displayPath;
    file.sourcePath = projectFile.path;
    file.lines = readSignificantLines(source.read(projectFile.path));

    if (file.lines.size() < options.minLines)
      continue;

    file.covered.assign(file.lines.size(), 0);
    corpus.files.push_back(std::move(file));
  }

  return corpus;
}

[[nodiscard]] std::unordered_set<std::string> collectChangedFiles(const Options &options,
                                                                  const fs::path &repoRoot,
                                                                  const std::string &scopePrefix,
                                                                  const std::string &baselineRef,
                                                                  const std::string &currentRef)
{
  const ExcludeMatcher matcher(options.excludePatterns);
  std::unordered_set<std::string> out;

  const auto changed = archcheck::git::changedCppFiles(repoRoot, baselineRef, currentRef);
  if (!changed.has_value())
    throw std::runtime_error("git diff failed while collecting changed C/C++ files");

  for (const fs::path &path : *changed)
  {
    const std::string repoRelativePath = path.generic_string();
    if (!pathInScope(repoRelativePath, scopePrefix))
      continue;

    const std::string displayPath = makeDisplayPath(repoRelativePath, scopePrefix);
    if (matcher.matches(displayPath))
      continue;

    out.insert(displayPath);
  }

  return out;
}

[[nodiscard]] InputCorpus loadCorpusForRef(const Options &options, const GitMode &gitMode, const std::string &ref)
{
  if (ref == archcheck::git::kWorktreeRef)
    return loadFilesystemCorpus(options);

  return loadGitCorpus(options, gitMode.repoRoot, gitMode.scopePrefix, ref);
}

[[nodiscard]] GitMode resolveGitMode(const Options &options)
{
  if (!options.gitDiff && !options.gitCommit)
    throw std::runtime_error("git mode not requested");

  if (options.gitDiff && options.gitCommit)
    throw std::runtime_error("--git-diff and --git-commit are mutually exclusive");

  const auto repoRoot = archcheck::git::findRepoRoot(options.root);
  if (!repoRoot.has_value())
    throw std::runtime_error("root is not inside a git repository");

  GitMode gitMode;
  gitMode.repoRoot = *repoRoot;
  gitMode.scopePrefix = computeScopePrefix(options.root, *repoRoot);

  if (options.gitCommit)
  {
    gitMode.baselineRef = *options.gitCommit + "^";
    gitMode.currentRef = *options.gitCommit;
  }
  else
  {
    const auto revspec = archcheck::git::parseRevspec(*options.gitDiff);
    if (!revspec.has_value())
      throw std::runtime_error("invalid git revspec: " + *options.gitDiff);

    gitMode.baselineRef = revspec->baseline;
    gitMode.currentRef = revspec->current;
  }

  gitMode.changedFiles =
      collectChangedFiles(options, gitMode.repoRoot, gitMode.scopePrefix, gitMode.baselineRef, gitMode.currentRef);
  return gitMode;
}

[[nodiscard]] BlockKey makeBlockKey(const Block &block)
{
  return {block.fileA, block.fileB, block.fingerprint};
}

[[nodiscard]] std::unordered_map<BlockKey, std::size_t, BlockKeyHash> countVisibleBlocks(const std::vector<Block> &blocks,
                                                                                          const Options &options)
{
  std::unordered_map<BlockKey, std::size_t, BlockKeyHash> counts;
  for (const Block &block : blocks)
  {
    if (!shouldShowBlock(block, options))
      continue;
    ++counts[makeBlockKey(block)];
  }
  return counts;
}

[[nodiscard]] std::vector<Block> collectIntroducedBlocks(const Result &baseline,
                                                         Result current,
                                                         const Options &options,
                                                         const std::unordered_set<std::string> &changedFiles)
{
  const auto baselineCounts = countVisibleBlocks(baseline.blocks, options);
  std::unordered_map<BlockKey, std::vector<std::size_t>, BlockKeyHash> currentGroups;

  for (std::size_t index = 0; index < current.blocks.size(); ++index)
  {
    Block &block = current.blocks[index];
    block.touchesChanged = changedFiles.find(block.fileA) != changedFiles.end() ||
                           changedFiles.find(block.fileB) != changedFiles.end();

    if (!shouldShowBlock(block, options))
      continue;

    currentGroups[makeBlockKey(block)].push_back(index);
  }

  std::vector<Block> introduced;
  for (const auto &entry : currentGroups)
  {
    const BlockKey &key = entry.first;
    const auto &indices = entry.second;

    const auto baselineIt = baselineCounts.find(key);
    const std::size_t baselineCount = (baselineIt == baselineCounts.end()) ? 0 : baselineIt->second;
    if (indices.size() <= baselineCount)
      continue;

    std::size_t delta = indices.size() - baselineCount;
    for (std::size_t index : indices)
    {
      Block &block = current.blocks[index];
      if (!block.touchesChanged)
        continue;

      block.introduced = true;
      introduced.push_back(block);
      if (--delta == 0)
        break;
    }
  }

  std::sort(introduced.begin(), introduced.end(), [](const Block &lhs, const Block &rhs) {
    if (lhs.length != rhs.length)
      return lhs.length > rhs.length;
    if (lhs.fileA != rhs.fileA)
      return lhs.fileA < rhs.fileA;
    if (lhs.fileB != rhs.fileB)
      return lhs.fileB < rhs.fileB;
    return lhs.startA < rhs.startA;
  });

  return introduced;
}

[[nodiscard]] CommitDiffReport detectCommitIntroducedBlocks(const Options &options, const GitMode &gitMode)
{
  CommitDiffReport report;
  report.baseline = detect(loadCorpusForRef(options, gitMode, gitMode.baselineRef), options);
  report.current = detect(loadCorpusForRef(options, gitMode, gitMode.currentRef), options);
  report.introducedBlocks = collectIntroducedBlocks(report.baseline, report.current, options, gitMode.changedFiles);
  return report;
}

void printUsage(std::ostream &out)
{
  out << "Usage: line_duplication <root> [options]\n"
      << "Options:\n"
      << "  --min-lines N          Minimum duplicated window size (default: 5)\n"
      << "  --min-window-chars N   Ignore windows shorter than N chars (default: 60)\n"
      << "  --top N                Show top N duplicated blocks (default: 8)\n"
      << "  --exclude PATTERN      Add exclude pattern (repeatable)\n"
      << "  --cross-only           Show only cross-file blocks in the top list\n"
      << "  --git-diff A..B        Compare baseline/current git refs via blobs\n"
      << "  --git-diff A           Compare git ref A against current working tree\n"
      << "  --git-commit SHA       Shorthand for SHA^..SHA\n"
      << "  --help                 Show this help\n";
}

[[nodiscard]] std::size_t parseUnsigned(const std::string &value, const char *flag)
{
  try
  {
    return std::stoull(value);
  }
  catch (const std::exception &)
  {
    throw std::runtime_error(std::string("invalid numeric value for ") + flag + ": " + value);
  }
}

[[nodiscard]] Options parseArgs(int argc, char **argv)
{
  if (argc < 2)
    throw std::runtime_error("missing root path");

  Options options;
  bool rootSet = false;

  for (int index = 1; index < argc; ++index)
  {
    const std::string arg = argv[index];

    if (arg == "--help")
    {
      printUsage(std::cout);
      std::exit(0);
    }

    if (arg == "--min-lines" || arg == "--min-window-chars" || arg == "--top" || arg == "--exclude" ||
        arg == "--git-diff" || arg == "--git-commit")
    {
      if (index + 1 >= argc)
        throw std::runtime_error("missing value after " + arg);

      const std::string value = argv[++index];
      if (arg == "--min-lines")
        options.minLines = parseUnsigned(value, "--min-lines");
      else if (arg == "--min-window-chars")
        options.minWindowChars = parseUnsigned(value, "--min-window-chars");
      else if (arg == "--top")
        options.top = parseUnsigned(value, "--top");
      else if (arg == "--exclude")
        options.excludePatterns.push_back(value);
      else if (arg == "--git-diff")
        options.gitDiff = value;
      else
        options.gitCommit = value;

      continue;
    }

    if (arg == "--cross-only")
    {
      options.crossOnly = true;
      continue;
    }

    if (startsWith(arg, "--"))
      throw std::runtime_error("unknown option: " + arg);

    if (rootSet)
      throw std::runtime_error("multiple root paths provided");

    options.root = fs::path(arg);
    rootSet = true;
  }

  if (!rootSet)
    throw std::runtime_error("missing root path");

  if (options.minLines == 0)
    throw std::runtime_error("--min-lines must be > 0");

  if (!fs::exists(options.root) || !fs::is_directory(options.root))
    throw std::runtime_error("root is not a directory: " + options.root.string());

  return options;
}

void printTopBlocks(const std::vector<Block> &blocks, const Options &options, std::string_view title)
{
  std::size_t shown = 0;
  std::cout << "\n" << title << ":\n";

  for (const Block &block : blocks)
  {
    if (!shouldShowBlock(block, options))
      continue;

    const char *tag = isCrossFile(block) ? "CROSS-FILE" : "same-file ";
    std::cout << "  [" << tag << "] " << block.length << " lines  " << block.fileA << ":" << block.startA
              << "  <->  " << block.fileB << ":" << block.startB;

    if (block.introduced)
      std::cout << "  [INTRODUCED]";

    std::cout << "\n";

    if (++shown == options.top)
      break;
  }

  if (shown == 0)
    std::cout << "  (no blocks)\n";
}

void printSnapshotReport(const Result &result, const Options &options)
{
  const std::size_t crossFileBlocks = std::count_if(result.blocks.begin(), result.blocks.end(), isCrossFile);

  std::cout << "repo:              " << options.root << "\n";
  std::cout << "discovered files:  " << result.discoveredFiles << "\n";
  std::cout << "eligible files:    " << result.eligibleFiles << "\n";
  std::cout << "significant LOC:   " << result.totalSigLines << "\n";
  std::cout << "duplicated LOC:    " << result.coveredSigLines << "\n";
  std::cout << "duplication ratio: " << result.duplicationRatio * 100.0 << "%  "
            << "(min block = " << options.minLines << " lines)\n";
  std::cout << "duplicated blocks: " << result.blocks.size() << "\n";
  std::cout << "cross-file blocks: " << crossFileBlocks << "\n";

  printTopBlocks(result.blocks, options, "top duplicated blocks");
}

void printCommitReport(const CommitDiffReport &report, const GitMode &gitMode, const Options &options)
{
  const std::size_t baselineCross = std::count_if(report.baseline.blocks.begin(), report.baseline.blocks.end(), isCrossFile);
  const std::size_t currentCross = std::count_if(report.current.blocks.begin(), report.current.blocks.end(), isCrossFile);
  const std::size_t introducedCross =
      std::count_if(report.introducedBlocks.begin(), report.introducedBlocks.end(), isCrossFile);

  std::cout << "repo:                        " << options.root << "\n";
  std::cout << "git baseline:                " << gitMode.baselineRef << "\n";
  std::cout << "git current:                 " << gitMode.currentRef << "\n";
  std::cout << "changed C/C++ files:         " << gitMode.changedFiles.size() << "\n";
  std::cout << "baseline duplication ratio:  " << report.baseline.duplicationRatio * 100.0 << "%\n";
  std::cout << "current duplication ratio:   " << report.current.duplicationRatio * 100.0 << "%\n";
  std::cout << "baseline cross-file blocks:  " << baselineCross << "\n";
  std::cout << "current cross-file blocks:   " << currentCross << "\n";
  std::cout << "introduced blocks in diff:   " << report.introducedBlocks.size() << "\n";
  std::cout << "introduced cross-file:       " << introducedCross << "\n";

  printTopBlocks(report.introducedBlocks, options, "top introduced duplicated blocks");
}

} // namespace

int main(int argc, char **argv)
{
  try
  {
    const Options options = parseArgs(argc, argv);

    if (options.gitDiff || options.gitCommit)
    {
      const GitMode gitMode = resolveGitMode(options);
      const CommitDiffReport report = detectCommitIntroducedBlocks(options, gitMode);
      printCommitReport(report, gitMode, options);
      return 0;
    }

    const Result result = detect(loadFilesystemCorpus(options), options);
    printSnapshotReport(result, options);
    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "ERROR: " << ex.what() << "\n";
    printUsage(std::cerr);
    return 1;
  }
}
