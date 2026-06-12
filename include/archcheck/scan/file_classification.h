#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

// Single source of truth for the file/dir classification defaults shared by
// every file source (disk traversal + git object DB) and the text-scan rules.
// Task #041: keep these embedded defaults in one place instead of duplicating
// the arrays per translation unit (they had already drifted between the disk
// and git backends).
//
// NOTE: vendor / third_party trees are intentionally NOT excluded here —
// discoverFiles must still surface them (test "discover_files does NOT
// auto-exclude third_party / vendor"). Vendor exclusion is a graph/diff-layer
// concern, not a file-enumeration one.
namespace archcheck::scan
{

// Project source + header extensions recognised by the v0.1 scan.
inline constexpr std::array<std::string_view, 12> kProjectExtensions = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp", ".tpp", ".inl", ".inc",
};

// Header-only subset; text-scan rules (SF.7, SF.8) apply only to these.
inline constexpr std::array<std::string_view, 8> kHeaderExtensions = {
    ".h", ".hh", ".hpp", ".hxx", ".ipp", ".tpp", ".inl", ".inc",
};

// Directory names skipped during traversal (exact match), plus the
// cmake-build-* prefix handled separately below.
inline constexpr std::array<std::string_view, 6> kExcludedDirNames = {
    ".git", "build", ".cache", ".idea", ".vscode", "out",
};
inline constexpr std::string_view kCmakeBuildPrefix = "cmake-build-";

// True if a single path segment names a directory the scan must not descend
// into. Shared by DiskFileSource (per-entry) and GitObjectFileSource (per path
// segment) so the exclusion set cannot drift between the two backends.
inline bool isExcludedDirName(std::string_view name)
{
  if (std::find(kExcludedDirNames.begin(), kExcludedDirNames.end(), name) != kExcludedDirNames.end())
  {
    return true;
  }
  return name.size() >= kCmakeBuildPrefix.size() && name.compare(0, kCmakeBuildPrefix.size(), kCmakeBuildPrefix) == 0;
}

// === Vendored code exclusion (#069 files, #068 dirs) =========================
// Vendored libraries are real text but not author drift: their cycles, god-files
// and copy-paste are noise in every signal. #068 caught vendor *directories*;
// #069 adds single-file libs dropped straight into src/ (no third_party/ dir).
// Canonical home shared by archcheck's graph builder and the #056 dedup spike.

inline std::string toLowerAscii(std::string_view s)
{
  std::string out(s);
  for (char &c : out)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

// Canonical directory-segment spelling: lowercased with '_'/'-'/space removed,
// so third_party / 3rd-party / ThirdParty and unit_test / unit-tests / UnitTests
// each collapse to one form. Shared by the vendored- and test-dir classifiers.
inline std::string normalizeDirSegment(std::string_view name)
{
  std::string norm;
  norm.reserve(name.size());
  for (char c : name)
  {
    if (c == '_' || c == '-' || c == ' ')
    {
      continue;
    }
    norm.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return norm;
}

// True if any *directory* segment of a repo-relative POSIX path satisfies
// `pred` (the final, file-name segment is never tested). Shared walker so the
// vendored- and test-dir checks cannot drift apart.
inline bool pathAnyDirSegment(std::string_view path, bool (*pred)(std::string_view))
{
  std::size_t start = 0;
  while (start < path.size())
  {
    const std::size_t slash = path.find('/', start);
    if (slash == std::string_view::npos)
    {
      return false; // last segment = file name, skip
    }
    if (pred(path.substr(start, slash - start)))
    {
      return true;
    }
    start = slash + 1;
  }
  return false;
}

// Vendored dependency directories (#068). Name is normalised (lowercased,
// '_'/'-'/space stripped) so third_party / 3rd-party / node_modules / ThirdParty
// all collapse to one canonical spelling.
inline constexpr std::array<std::string_view, 14> kVendoredDirNames = {
    "thirdparty", "3rdparty", "vendor",       "vendored",   "vendors",   "external",    "externals",
    "extern",     "deps",     "dependencies", "submodules", "submodule", "nodemodules", "contrib",
};

// Well-known multi-file libraries vendored straight into the tree under their own
// directory (src/qhull/, source/Irrlicht/jpeglib/, src/agg/, ...) rather than a
// third_party/ wrapper — so the container-name list above misses them, and they
// are not single-file libs either. Curated like kVendoredStems: distinctive,
// rarely-an-author's-own-module names confirmed vendored in the #056 corpus scan.
// Matched as a directory segment (normalized: lowercased, '_'/'-'/space removed).
inline constexpr std::array<std::string_view, 14> kVendoredLibDirs = {
    "jpeglib", "libjpeg", "libpng",     "zlib", "bzip2",    "qhull", "hidapi",
    "libigl",  "agg",     "glulibtess", "mcut", "freetype", "glfw",  "glew",
};

inline bool isVendoredDirName(std::string_view name)
{
  const std::string norm = normalizeDirSegment(name);
  if (std::find(kVendoredDirNames.begin(), kVendoredDirNames.end(), norm) != kVendoredDirNames.end() ||
      std::find(kVendoredLibDirs.begin(), kVendoredLibDirs.end(), norm) != kVendoredLibDirs.end())
  {
    return true;
  }
  // `zlib-1.3.2` normalizes to `zlib1.3.2`; a dotted version tail maps back to
  // the plain library name (#109 corpus: vendored zlib update flagged 32 times).
  const std::size_t tail = norm.find_last_not_of("0123456789.");
  if (tail == std::string::npos || norm.find('.', tail + 1) == std::string::npos)
  {
    return false;
  }
  const std::string_view stem = std::string_view{norm}.substr(0, tail + 1);
  return std::find(kVendoredLibDirs.begin(), kVendoredLibDirs.end(), stem) != kVendoredLibDirs.end();
}

// True if any *directory* segment of a repo-relative POSIX path is vendored
// (the final, file-name segment is not tested).
inline bool pathHasVendoredDir(std::string_view path) { return pathAnyDirSegment(path, isVendoredDirName); }

inline std::string_view baseName(std::string_view path)
{
  const std::size_t slash = path.rfind('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// Layer 1 — curated single-file-lib basenames (case-insensitive). Base:
// nothings/single_file_libs + #054 corpus scan. Stems match any source/header
// extension (qcustomplot.cpp/.h); kVendoredExact pin a full basename.
inline constexpr std::array<std::string_view, 7> kVendoredStems = {
    "qcustomplot", "sqlite3", "miniz", "tinyxml2", "pugixml", "lodepng", "cjson",
};
inline constexpr std::array<std::string_view, 12> kVendoredExact = {
    "json.hpp", "httplib.h",   "lua.c",      "doctest.h",  "catch.hpp", "glad.c",
    "entt.hpp", "miniaudio.h", "uni_algo.h", "vulkan.hpp", "td_api.h",  "nuklear.h",
};

inline bool isVendoredBasename(std::string_view filename)
{
  const std::string name = toLowerAscii(filename);
  // stb_*.h and imgui*.cpp families — prefix on the basename.
  if (name.rfind("stb_", 0) == 0 || name.rfind("imgui", 0) == 0)
  {
    return true;
  }
  const std::size_t dot = name.rfind('.');
  const std::string_view stem =
      (dot == std::string::npos) ? std::string_view{name} : std::string_view{name}.substr(0, dot);
  if (std::find(kVendoredStems.begin(), kVendoredStems.end(), stem) != kVendoredStems.end())
  {
    return true;
  }
  return std::find(kVendoredExact.begin(), kVendoredExact.end(), std::string_view{name}) != kVendoredExact.end();
}

// Layer 2 — license-header heuristic. Renamed / unknown vendors still carry a
// permissive-license banner in the first ~40 lines. Only *full verbatim* license
// texts count as a vendor signal: authored projects using SPDX paste a one-line
// `SPDX-License-Identifier:` tag, not the full text, so the SPDX tag alone is NOT
// a vendor marker (#081 — KDE/SPDX-GPL repos were 0-scanned by over-exclusion).
inline bool hasVendorLicenseHeader(std::string_view headerBytes)
{
  const std::string head = toLowerAscii(headerBytes.substr(0, std::min<std::size_t>(headerBytes.size(), 2000)));
  static constexpr std::array<std::string_view, 6> kLicenseMarkers = {
      "permission is hereby granted",     // MIT
      "redistribution and use in source", // BSD
      "released into the public domain",  // stb / public-domain
      "licensed under the apache",        // Apache-2.0
      "boost software license",           // Boost
      "zlib license",                     // zlib/libpng
  };
  for (std::string_view m : kLicenseMarkers)
  {
    if (head.find(m) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

// Combined file-level vendor test (layers 1 + 2). `filename` is the basename;
// `headerBytes` the first bytes of the file (license banners sit at the top).
inline bool isVendoredFile(std::string_view filename, std::string_view headerBytes)
{
  return isVendoredBasename(filename) || hasVendorLicenseHeader(headerBytes);
}

// === Unit/integration test exclusion (#070) ==================================
// Test code duplicates by nature — parallel cases, shared fixtures,
// CHECK()-boilerplate — and its cycles / god-headers are not author drift. It is
// dropped from every signal alongside vendored code: directory segments
// test/ tests/ testutil/ testutils/ unit_test(s)/, plus basenames foo_test.* / foo_tests.* /
// test_foo.* / test-foo.* / foo-test.* / foo_spec.* / foo-spec.*.
inline constexpr std::array<std::string_view, 6> kTestDirNames = {
    "test", "tests", "testutil", "testutils", "unittest", "unittests",
};

inline bool isTestDirName(std::string_view name)
{
  const std::string norm = normalizeDirSegment(name);
  return std::find(kTestDirNames.begin(), kTestDirNames.end(), norm) != kTestDirNames.end();
}

inline bool pathHasTestDir(std::string_view path) { return pathAnyDirSegment(path, isTestDirName); }

// Test-file basename heuristic on the stem: starts with test_ or test-, or ends with
// _test / _tests / _spec / -test / -tests / -spec (case-insensitive).
// GCC8-COMPAT: libstdc++8 has no std::string_view::ends_with — match via
// compare() like the vendored helpers above.
inline bool isTestBasename(std::string_view filename)
{
  const std::string name = toLowerAscii(filename);
  const std::size_t dot = name.rfind('.');
  const std::string stem = (dot == std::string::npos) ? name : name.substr(0, dot);
  if (stem.rfind("test_", 0) == 0)
  {
    return true;
  }
  if (stem.rfind("test-", 0) == 0)
  {
    return true;
  }
  static constexpr std::array<std::string_view, 6> kTestStemSuffixes = {"_test", "_tests", "_spec",
                                                                        "-test", "-tests", "-spec"};
  for (std::string_view suffix : kTestStemSuffixes)
  {
    if (stem.size() >= suffix.size() && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
      return true;
    }
  }
  return false;
}

} // namespace archcheck::scan
