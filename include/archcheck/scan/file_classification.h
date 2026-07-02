#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

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

// === #154 Phase 2: additive runtime overrides from .archcheck.yml ============
// Default-empty ⇒ zero-config behaviour is exactly the curated constexpr defaults
// below. Loaded ONCE at startup from Config (the CLI calls setClassificationExtras
// before any scan); the curated defaults are never removed, only extended with
// project-specific tokens. Stored values are pre-normalized by the caller: dir
// names via normalizeDirSegment, markers via toLowerAscii.
struct ClassificationExtras
{
  std::unordered_set<std::string> vendoredDirs;     // normalizeDirSegment-normalized
  std::unordered_set<std::string> testDirs;         // normalizeDirSegment-normalized
  std::unordered_set<std::string> generatedMarkers; // lowercased path substrings
};

inline ClassificationExtras &classificationExtrasStorage()
{
  static ClassificationExtras storage;
  return storage;
}
inline const ClassificationExtras &classificationExtras() { return classificationExtrasStorage(); }
inline void setClassificationExtras(ClassificationExtras extras) { classificationExtrasStorage() = std::move(extras); }

// Project source + header extensions recognised by the v0.1 scan.
// `.C` (uppercase) is the traditional GCC spelling for C++ source, distinct from
// `.c` on case-sensitive filesystems (#131 corpus: 5 rows unscanned without it).
inline constexpr std::array<std::string_view, 13> kProjectExtensions = {
    ".c", ".C", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp", ".tpp", ".inl", ".inc",
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
// `externallibraries` (#127): supercollider ships bcp-trimmed Boost AND ICU under
// `external_libraries/` — a container the bare `external` token misses (normalize
// strips '_' → `externallibraries`, not `external`). Name-independent, so it drops
// both subtrees at once in disk and diff modes.
inline constexpr std::array<std::string_view, 16> kVendoredDirNames = {
    "thirdparty", "3rdparty",     "vendor",     "vendored",  "vendors",     "external", "externals",
    "extern",     "dep", // microsoft/terminal ships its vendored headers under dep/
    "deps",       "dependencies", "submodules", "submodule", "nodemodules", "contrib",  "externallibraries",
};

// Well-known multi-file libraries vendored straight into the tree under their own
// directory (src/qhull/, source/Irrlicht/jpeglib/, src/agg/, ...) rather than a
// third_party/ wrapper — so the container-name list above misses them, and they
// are not single-file libs either. Curated like kVendoredStems: distinctive,
// rarely-an-author's-own-module names confirmed vendored in the #056 corpus scan.
// Matched as a directory segment (normalized: lowercased, '_'/'-'/space removed).
inline constexpr std::array<std::string_view, 15> kVendoredLibDirs = {
    "jpeglib", "libjpeg", "libpng",     "zlib", "bzip2",    "qhull", "hidapi",
    "libigl",  "agg",     "glulibtess", "mcut", "freetype", "glfw",  "glew",
    "fmt", // bundled {fmt} headers, e.g. btop include/fmt/ (#164 B.1)
};

// Self-project guard: running archcheck ON a library from kVendoredLibDirs must
// not vendor-drop the project's own code — the {fmt} repo keeps its headers at
// include/fmt/, the same shape as a bundled copy inside btop. Path shape cannot
// separate the two; the scan root's own directory name can. File sources register
// it on construction; a curated lib token equal to it stops counting as vendored
// for that run. Container names (third_party/, dep/, ...) are never exempt.
inline std::string &selfProjectDirStorage()
{
  static std::string storage;
  return storage;
}
inline void setSelfProjectDir(std::string_view rootDirName)
{
  selfProjectDirStorage() = normalizeDirSegment(rootDirName);
}

inline bool isVendoredLibDirName(std::string_view norm)
{
  return norm != selfProjectDirStorage() &&
         std::find(kVendoredLibDirs.begin(), kVendoredLibDirs.end(), norm) != kVendoredLibDirs.end();
}

inline bool isVendoredDirName(std::string_view name)
{
  const std::string norm = normalizeDirSegment(name);
  if (std::find(kVendoredDirNames.begin(), kVendoredDirNames.end(), norm) != kVendoredDirNames.end() ||
      isVendoredLibDirName(norm) || classificationExtras().vendoredDirs.count(norm) != 0)
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
  return isVendoredLibDirName(std::string_view{norm}.substr(0, tail + 1));
}

// True if any *directory* segment of a repo-relative POSIX path is vendored
// (the final, file-name segment is not tested).
inline bool pathHasVendoredDir(std::string_view path) { return pathAnyDirSegment(path, isVendoredDirName); }

// Exact-suffix matches (not loose substrings, so `socket_wrap.cpp` is safe): SWIG
// `*_wrap.*`, and the upb amalgamation `*-upb.c/.h` (#151 — all of upb in one file).
inline bool hasGeneratedSuffix(const std::string &lower)
{
  static constexpr std::array<std::string_view, 5> kGeneratedSuffixes = {
      "_wrap.cpp", "_wrap.cxx", "_wrap.c", "-upb.c", "-upb.h",
  };
  for (const std::string_view suf : kGeneratedSuffixes)
  {
    if (lower.size() >= suf.size() && lower.compare(lower.size() - suf.size(), suf.size(), suf) == 0)
    {
      return true;
    }
  }
  return false;
}

// Substring markers: protobuf/upb (.pb.*, .upb.), Qt (moc_/ui_/qrc_), flex/bison/lemon
// (.tab.*, lex.yy, lempar), `_generated`/`/generated/` conventions (#151 arrow, #127), plus
// any project-specific markers from .archcheck.yml `classification:` (#154 Phase 2).
inline bool hasGeneratedMarker(const std::string &lower)
{
  static constexpr std::array<std::string_view, 16> kGeneratedMarkers = {
      ".pb.h", ".pb.cc", ".pb.cpp", "_generated.", ".generated.", "/generated/", "/moc_",       "/ui_",
      "/qrc_", ".tab.c", ".tab.h",  "lex.yy",      ".g.cpp",      ".upb.",       "_generated_", "lempar",
  };
  for (const std::string_view m : kGeneratedMarkers)
  {
    if (lower.find(m) != std::string_view::npos)
    {
      return true;
    }
  }
  for (const std::string &m : classificationExtras().generatedMarkers)
  {
    if (lower.find(m) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

// Machine-generated files — protobuf, Qt moc/ui/qrc, flex/bison, SWIG wrappers —
// are never hand-edited, so duplication / complexity in them is not actionable.
// Matched on the lowercased repo-relative path. (#129: lifted from the dedup
// scanner's private copy so clone, complexity and graph share ONE definition.)
inline bool isGeneratedPath(std::string_view path)
{
  const std::string lower = toLowerAscii(path);
  return hasGeneratedSuffix(lower) || hasGeneratedMarker(lower);
}

inline std::string_view baseName(std::string_view path)
{
  const std::size_t slash = path.rfind('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// === Header-companion file roles (#154 consolidation) ========================
// Lifted from per-rule private copies so the SF.9 cycle rule and the god-header
// rule share ONE definition instead of each re-deciding these file roles. Both
// preserve the rules' original case-sensitive matching.

// Inline/template implementation files that legitimately pair with a same-stem
// header (foo.h + foo.inl / foo.ipp / foo-inl.h / foo_impl.hpp ...). The _impl.*
// family is the dominant header-only convention (mlpack, Boost, Eigen). Was a
// private list in src/rules/sf9_no_cycles.cpp.
inline constexpr std::array<std::string_view, 14> kImplSuffixes = {
    "-inl.h", "_inl.h", ".tmpl.h", ".impl.h",   ".inl",     ".ipp",    ".icc",
    ".tcc",   ".tpp",   ".hxx",    "_impl.hpp", "_impl.hh", "_impl.h", "_impl.hxx",
};

inline bool isInlineImplFile(std::string_view name)
{
  for (const std::string_view suf : kImplSuffixes)
  {
    if (name.size() >= suf.size() && name.compare(name.size() - suf.size(), suf.size(), suf) == 0)
    {
      return true;
    }
  }
  return false;
}

// Precompiled-header basenames exempt from the god-header fan-in rule. Exact
// (case-sensitive) basename match. Was a private set in
// src/rules/lakos_god_headers.cpp.
inline constexpr std::array<std::string_view, 4> kPchBasenames = {
    "pch.h",
    "stdafx.h",
    "precompiled.h",
    "precompiled_header.h",
};

inline bool isKnownPchBasename(std::string_view filename)
{
  return std::find(kPchBasenames.begin(), kPchBasenames.end(), filename) != kPchBasenames.end();
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

inline bool hasFlexGeneratedSignature(std::string_view head)
{
  return head.find("#define flex_scanner") != std::string::npos &&
         head.find("yy_flex_major_version") != std::string::npos;
}

inline bool hasGeneratedBannerMarker(std::string_view head)
{
  static constexpr std::array<std::string_view, 6> kBannerMarkers = {
      "coco/r",             // "Compiler Generator Coco/R" parser/scanner banner
      "linux-syscall-note", // kernel UAPI SPDX exception (copied kernel headers)
      "a bison parser, made by gnu bison",
      "skeleton implementation for bison",
      "skeleton interface for bison",
      "a lexical scanner generated by flex",
  };
  for (std::string_view m : kBannerMarkers)
  {
    if (head.find(m) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

// Generated / copied-in code with a name-independent path (#127). Catches what the
// path markers (isGeneratedPath) and dir tokens miss: tools that stamp a banner but
// keep an ordinary basename (Coco/R Parser.h/Scanner.h) and kernel UAPI headers copied
// into a tree (Linux-syscall-note). Unlike hasVendorLicenseHeader this is NOT guarded
// by the dominant-banner ratio: these markers are per-file truth, not a project-wide
// license style.
//
// The set is deliberately narrow and distinctive (near-zero false positives). Loose
// phrases that read as ordinary PROSE are intentionally ABSENT — they over-exclude
// authored code: "generated by" (bpftrace bpfbytecode.h: "bytecode generated by
// bpftrace"), "imported from" (functions.h), "do not edit", and "copied from" — a
// corpus check (#131) found "copied from" matched 82/91 times OUTSIDE any vendor
// signal (own-code FP on libjxl) for one unique catch; not worth the prose surface.
inline bool hasGeneratedHeader(std::string_view headerBytes)
{
  const std::string head = toLowerAscii(headerBytes.substr(0, std::min<std::size_t>(headerBytes.size(), 2000)));
  // Precise, position-free: a file carrying these IS generated/copied wherever it sits.
  if (hasGeneratedBannerMarker(head))
    return true;

  // `@generated` ONLY as a top-of-file banner tag, not a mid-file mention. A bare
  // substring (the old form) excluded hand-written code that emits/mentions the tag:
  // protobuf's compiler ("emit @Generated annotations", offset 1124) and an ORM emitter
  // ("@GeneratedValue", offset 543) — silent own-code loss. Genuine banners sit in the
  // first comment block (crubit/RN/folly offset <= 256) and the tag stands alone, not
  // as a prefix of an identifier. Corpus #131: 261/266 matches pass both; the 5 dropped
  // were the deep/identifier tail (protobuf compiler + ORM emitter confirmed own code).
  const std::size_t at = head.find("@generated");
  if (at != std::string::npos && at <= 256)
  {
    const char after = at + 10 < head.size() ? head[at + 10] : '\n';
    const bool identChar = (after >= 'a' && after <= 'z') || (after >= '0' && after <= '9') || after == '_';
    return !identChar;
  }
  return hasFlexGeneratedSignature(head);
}

// Minified / embedded-data heuristic (GitHub Linguist, #147/#127). A file whose average
// line length is far above a normal source line is generated/minified/embedded data, not
// hand-written code — e.g. a multi-MB vocab `.hpp` that is one giant
// `static const unsigned char x[] = {0x7b,0x22,...}` line (umt5.hpp: one 2.5M-char line).
// Excluded from all analysis like a vendored/generated file: tokenizing it builds a multi-GB
// index for zero clone/complexity value, and its byte literals are noise to every scan.
// Walked over a 64 KiB prefix only, so a 43 MiB blob is never traversed end-to-end: real
// source has many newlines in that span (avg line ~30-60), a single-line data blob ~none.
inline bool hasMinifiedContent(std::string_view content)
{
  constexpr std::size_t kSample = 64 * 1024;    // peek window — bounds cost on huge blobs
  constexpr std::size_t kMinBytes = 4096;       // small files carry no memory/noise risk
  constexpr std::size_t kAvgLineLenLimit = 110; // Linguist's minified threshold
  if (content.size() < kMinBytes)
  {
    return false;
  }
  const std::size_t span = std::min<std::size_t>(content.size(), kSample);
  std::size_t newlines = 0;
  for (std::size_t i = 0; i < span; ++i)
  {
    if (content[i] == '\n')
    {
      ++newlines;
    }
  }
  return span / (newlines + 1) > kAvgLineLenLimit;
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
// test/ tests/ tst/ testutil/ testutils/ unit_test(s)/, plus basenames foo_test.* /
// foo_tests.* / test_foo.* / test-foo.* / foo-test.* / foo_spec.* / foo-spec.*.
// `tst` is a common test-dir abbreviation (corpus: 8 repos, all test trees; e.g.
// dsstne's tst/gputests/). Deliberately NOT added: `units` (16 corpus repos, often
// units-of-measurement, not tests) and substring/endswith-"test" matching (would
// catch `latest/`, `contests/`) — over-exclusion silently hides real findings.
inline constexpr std::array<std::string_view, 9> kTestDirNames = {
    "test",     "tests",     "tst",     "testutil", "testutils",
    "unittest", "unittests", "testlib", "testlibs", // test-support libraries (zera-classes, papi, gem5; #164 B.3)
};

// CamelCase test boundary on a raw (un-lowercased) stem/segment: ends with capital
// `Test`/`Tests` preceded by a lowercase ascii letter (FooTest, EngineTests). The
// capital `T` + lowercase-before is what excludes latest/contest/attestation. The
// size guard keeps one char ahead of the suffix so the "before" read is in bounds.
inline bool hasCamelCaseTestSuffix(std::string_view stem)
{
  if (stem.size() >= 6 && stem.compare(stem.size() - 5, 5, "Tests") == 0)
  {
    return stem[stem.size() - 6] >= 'a' && stem[stem.size() - 6] <= 'z';
  }
  if (stem.size() >= 5 && stem.compare(stem.size() - 4, 4, "Test") == 0)
  {
    return stem[stem.size() - 5] >= 'a' && stem[stem.size() - 5] <= 'z';
  }
  return false;
}

// Lowercased stem ends with a separator-delimited test marker (_test, -spec, ...).
inline bool hasTestStemSuffix(std::string_view stem)
{
  static constexpr std::array<std::string_view, 6> kSuffixes = {"_test", "_tests", "_spec", "-test", "-tests", "-spec"};
  for (std::string_view s : kSuffixes)
  {
    if (stem.size() >= s.size() && stem.compare(stem.size() - s.size(), s.size(), s) == 0)
    {
      return true;
    }
  }
  return false;
}

inline bool isTestDirName(std::string_view name)
{
  if (hasCamelCaseTestSuffix(name)) // A.2: EngineTests/, WidgetTest/ (raw segment)
  {
    return true;
  }
  const std::string norm = normalizeDirSegment(name);
  return std::find(kTestDirNames.begin(), kTestDirNames.end(), norm) != kTestDirNames.end() ||
         classificationExtras().testDirs.count(norm) != 0;
}

inline bool pathHasTestDir(std::string_view path) { return pathAnyDirSegment(path, isTestDirName); }

// Test-file basename heuristic on the stem: starts with test_ or test-, or ends with
// _test / _tests / _spec / -test / -tests / -spec (case-insensitive), or ends with
// CamelCase Test/Tests suffix (with a preceding lowercase letter), or contains
// .test. or .spec. infix.
// GCC8-COMPAT: libstdc++8 has no std::string_view::ends_with — match via
// compare() like the vendored helpers above.
inline bool isTestBasename(std::string_view filename)
{
  const std::string name = toLowerAscii(filename);
  const std::size_t dot = name.rfind('.');
  const std::string stem = (dot == std::string::npos) ? name : name.substr(0, dot);

  if (stem.rfind("test_", 0) == 0 || stem.rfind("test-", 0) == 0)
  {
    return true; // prefix
  }
  if (stem == "test" || stem == "tests")
  {
    return true; // bare stem (tests.c / test.c, common C idiom; #164 B.2)
  }
  if (hasTestStemSuffix(stem))
  {
    return true; // _test / _tests / _spec / -test / -tests / -spec
  }
  if (name.find(".test.") != std::string::npos || name.find(".spec.") != std::string::npos)
  {
    return true; // dotted infix (alu_trace.test.cpp, foo.spec.cc)
  }
  // CamelCase XxxTest(s) on the raw (un-lowercased) stem — capital T is the discriminator.
  const std::size_t rawDot = filename.rfind('.');
  const std::string_view rawStem = (rawDot == std::string_view::npos) ? filename : filename.substr(0, rawDot);
  return hasCamelCaseTestSuffix(rawStem);
}

} // namespace archcheck::scan
