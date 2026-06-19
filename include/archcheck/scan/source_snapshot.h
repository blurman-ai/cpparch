#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archcheck/scan/authored_scope.h"
#include "archcheck/scan/file_source.h"

// The single read+classify stage (#129). ONE place reads a whole tree once and
// classifies every file; the rules (graph, clone, complexity, SF.*) consume the
// in-memory result and no longer call FileSource::read() themselves.
//
// Before this, graph / clone / complexity each listed + read + filtered the tree
// in their own loop (2-3x reads per ref via separate cat-file children) with
// divergent vendor/test/generated/banner formulas. SourceSnapshot reads each file
// once and runs the unified AuthoredScope over the whole set, so:
//   * the whole-tree dominant-banner ratio is computed once (no per-rule re-read),
//   * `authored` is a property of the file, identical for every rule, and
//   * diff-scoped rules (complexity) get the whole-tree classification for free
//     (so a vendored single-header is `authored=false` for them too — #129 rmm fix).
namespace archcheck::scan
{

struct SnapshotFile
{
  std::string path;
  std::string content;
  bool authored; // false => vendored / test / generated / non-dominant-banner
};

class SourceSnapshot
{
public:
  // Read+classify a whole tree once. The only place that calls source.read() over
  // the full listing.
  static SourceSnapshot read(FileSource &source)
  {
    std::vector<SnapshotFile> files;
    std::vector<std::pair<std::string, std::string>> forScope;
    for (const auto &f : source.list())
    {
      std::string content = source.read(f.path);
      forScope.push_back({f.path, content});
      files.push_back({f.path, std::move(content), /*authored=*/false});
    }
    const AuthoredScope scope = AuthoredScope::fromFiles(forScope);
    for (auto &sf : files)
    {
      sf.authored = !scope.excluded(sf.path, sf.content);
    }
    return SourceSnapshot{std::move(files)};
  }

  const std::vector<SnapshotFile> &files() const { return files_; }

  // Authored files as (path, content) pairs — the input shape the duplication
  // scanner expects. Replaces per-rule collectNonVendoredSources() loops.
  std::vector<std::pair<std::string, std::string>> authoredSources() const
  {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto &sf : files_)
    {
      if (sf.authored)
      {
        out.push_back({sf.path, sf.content});
      }
    }
    return out;
  }

  // The classified file at one path, or nullptr if absent. For diff-scoped rules
  // (complexity) that look up specific changed files and need both content and the
  // whole-tree `authored` verdict.
  const SnapshotFile *findFile(std::string_view path) const
  {
    for (const auto &sf : files_)
    {
      if (sf.path == path)
      {
        return &sf;
      }
    }
    return nullptr;
  }

private:
  explicit SourceSnapshot(std::vector<SnapshotFile> files) : files_(std::move(files)) {}
  std::vector<SnapshotFile> files_;
};

} // namespace archcheck::scan
