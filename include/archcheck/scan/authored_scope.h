#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archcheck/scan/file_classification.h"

// One shared "is this the project's own authored source?" gate (#129).
//
// Before this, the vendor/test/generated/banner AND-chain was open-coded at
// several call sites with DIVERGENT formulas, so the SAME file got DIFFERENT
// verdicts: clone's collectNonVendoredSources used an UNGUARDED banner (over-
// excludes self-licensed Apache repos), graph's filterVendored used a banner
// guarded by a >50%-dominant ratio, and complexity's collectFilePairs disabled
// the banner entirely (let vendored single-headers through -> false complexity).
//
// AuthoredScope is the one composition rung above the file_classification.h
// predicates. The license-banner layer needs a whole-file-set view: if >50% of
// the candidate files carry a full license banner it is the PROJECT's own
// license, not a vendor signal, so the banner layer is disabled (#109/#113
// foundationdb guard). `fromFiles` captures that ratio once. Since #129 every
// production consumer (graph, clone, complexity) reads the whole tree via
// SourceSnapshot and therefore uses `fromFiles`; `changedFilesMode` (banner term
// a no-op, for callers without the full set in hand) now survives only for the
// characterization tests and any future changed-files-only caller.
namespace archcheck::scan
{

class AuthoredScope
{
public:
  // Path+basename+generated layer — shared by every consumer, content-free.
  static bool pathExcluded(std::string_view path)
  {
    const std::string_view base = baseName(path);
    return pathHasVendoredDir(path) || pathHasTestDir(path) || isTestBasename(base) || isVendoredBasename(base) ||
           isGeneratedPath(path);
  }

  // Build over a known (path, content) set; the dominant-banner ratio is taken
  // over the files that survive the path+basename layer (matching filterVendored).
  static AuthoredScope fromFiles(const std::vector<std::pair<std::string, std::string>> &files)
  {
    std::size_t candidates = 0;
    std::size_t banners = 0;
    for (const auto &[path, content] : files)
    {
      if (pathExcluded(path))
      {
        continue;
      }
      ++candidates;
      if (hasVendorLicenseHeader(content))
      {
        ++banners;
      }
    }
    const bool dominant = candidates > 0 && banners * 2 > candidates;
    return AuthoredScope{/*bannerIsProjectOwn=*/dominant};
  }

  // Changed-files mode: the full file set is not available, so the banner term is
  // a no-op (cannot compute the dominant ratio). No production caller since #129
  // (all read the whole tree via SourceSnapshot ⇒ fromFiles); kept for the
  // characterization tests and any future changed-files-only consumer.
  static AuthoredScope changedFilesMode() { return AuthoredScope{/*bannerIsProjectOwn=*/true}; }

  // True => vendored / test / generated / non-dominant-banner => exclude from
  // author-drift analysis.
  bool excluded(std::string_view path, std::string_view content) const
  {
    if (pathExcluded(path))
    {
      return true;
    }
    return !bannerIsProjectOwn_ && hasVendorLicenseHeader(content);
  }

private:
  explicit AuthoredScope(bool bannerIsProjectOwn) : bannerIsProjectOwn_(bannerIsProjectOwn) {}
  bool bannerIsProjectOwn_ = true;
};

} // namespace archcheck::scan
