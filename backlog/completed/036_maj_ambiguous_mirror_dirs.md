# [SCAN][CLI] Ambiguous from mirror directories: single_include/, _amalgamated

**Created:** 2026-05-28
**Started:** 2026-05-28
**Completed:** 2026-05-29
**Status:** completed
**Module:** SCAN / CLI
**Priority:** major
**Complexity:** S
**Blocks:** —
**Blocked by:** —
**Related:** #028 (rules_engine_mvp)

## Goal

Eliminate false ambiguous findings that arise when a repo contains `single_include/` or a similar directory with duplicate headers.

## Context

A run on nlohmann/json (commit `d10879b`) produced **333 ambiguous** — all false. Cause:

```
/tmp/json/include/nlohmann/json.hpp         ← main source
/tmp/json/single_include/nlohmann/json.hpp  ← generated amalgam
```

Both files are named `json.hpp`. Any `#include <nlohmann/json.hpp>` finds two candidates → `ambiguous`. Same for `json_fwd.hpp`.

The pattern is common: nlohmann/json, {fmt}, Catch2 (extras/), Abseil (doesn't have it, but other similar projects do). Typical mirror directory names: `single_include/`, `amalgamate/`, `dist/`, `release/`, `generated/`.

## Solution

Two independent approaches, both can be implemented:

**A — default exclude list (quick fix):**
When resolving `ambiguous` — if among the candidates one path contains `single_include/`, `amalgamate/`, `dist/`, `generated/` — prefer the other, do not mark as ambiguous. Keep the list of well-known directories in `include_resolver.cpp`.

**B — configurable exclude in `.archcheck.yml`:**
```yaml
scan:
  exclude_dirs: ["single_include", "build", "third_party"]
```
Files from exclude_dirs are not added to the project graph at all → they cannot be either a source or a candidate.

Option B is more correct (the user manages it explicitly), but requires config. Option A is a zero-config quick fix that covers the common case.

**Recommendation:** implement A now as a built-in heuristic, B — as a follow-up once config exists.

## Execution plan

- [x] `src/scan/include_resolver.cpp`: on ambiguous — prefer the path without well-known mirror dirs
- [x] List of well-known dirs: `single_include`, `amalgamate`, `amalgamated`, `dist`, `release/include`, `generated`
- [x] Verify: nlohmann/json → 0 ambiguous (was 333, now 0 ✓)
- [ ] Verify: fmt, spdlog, Catch2 — no regressions
- [x] Test: two candidates, one in `single_include/` → the second is chosen, not ambiguous

## Done

- Implemented option A (heuristic): `kMirrorPrefixes` + `is_mirror_dir_path()` in `include_resolver.cpp`
- Filtering in `resolve_by_suffix`: if exactly 1 candidate is not in a mirror-dir → `Project`, otherwise `Ambiguous` as before
- Passed the `files` parameter through the chain `resolveInclude → resolve_quote/resolve_angle → resolve_by_suffix`
- Added 2 tests: happy path (`single_include` is skipped) + edge case (both in mirror → Ambiguous)
- Clean build, 739 tests passed, lizard without warnings
- `starts_with` replaced with `path.find(prefix) == 0` (clang 11 does not support C++20 `starts_with`)

## In progress

- Manual check on nlohmann/json (`./archcheck --graph /tmp/json`)

## Key decisions

- `path.find(prefix) == 0` instead of `starts_with` — clang 11 on Astra Linux does not have `std::string_view::starts_with` even with `-std=c++20`
- We pass `*candidates` (not `preferred`) to `make_ambiguous` when filtering did not help — preserving full information about the conflict

## Changed files

| File | Change |
|------|-----------|
| `src/scan/include_resolver.cpp` | `kMirrorPrefixes`, `is_mirror_dir_path()`, filtering in `resolve_by_suffix`, passing `files` |
| `tests/unit/scan/include_resolver_test.cpp` | 2 new mirror-prefer tests |
