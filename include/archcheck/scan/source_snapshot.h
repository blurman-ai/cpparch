#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
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
  // whole-tree `authored` verdict. O(1) via index_ — the diff path does one lookup
  // per changed file.
  const SnapshotFile *findFile(std::string_view path) const
  {
    const auto it = index_.find(path);
    return it == index_.end() ? nullptr : &files_[it->second];
  }

  // index_ aliases files_[i].path buffers, so the snapshot is non-copyable (a copy
  // would reallocate the paths and dangle the views). Moves are safe: the vector's
  // heap-stored elements keep their addresses, so the views stay valid.
  SourceSnapshot(const SourceSnapshot &) = delete;
  SourceSnapshot &operator=(const SourceSnapshot &) = delete;
  SourceSnapshot(SourceSnapshot &&) = default;
  SourceSnapshot &operator=(SourceSnapshot &&) = default;

private:
  explicit SourceSnapshot(std::vector<SnapshotFile> files) : files_(std::move(files))
  {
    index_.reserve(files_.size());
    for (std::size_t i = 0; i < files_.size(); ++i)
      index_.emplace(files_[i].path, i); // emplace keeps the first entry on a dup path
  }

  std::vector<SnapshotFile> files_;
  std::unordered_map<std::string_view, std::size_t> index_;
};

} // namespace archcheck::scan
