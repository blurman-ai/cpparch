# [RULES] GodHeader: exclude precompiled headers from the fan-in check

**Created:** 2026-05-28
**Started:** 2026-05-28
**Completed:** 2026-05-28
**Status:** completed
**Module:** RULES
**Priority:** minor
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** —

## Goal

Stop emitting a false positive `Lakos.GodHeader` on precompiled header files (`pch.h`, `stdafx.h`, etc.), which by definition are included in every translation unit.

## Context

When running archcheck on real projects (`gm/rmi`, `samastra_itain`), the `Lakos.GodHeader` rule fired on `pch.h` / `rmi/pch.h` with fan-in 60–111. This is a false positive: a precompiled header is deliberately included in every `.cpp`.

## How it works

In `LakosGodHeaders::check()`, before counting fan-in, `isPchName(path)` is called — a static method that takes `filename()` via `std::filesystem::path` and checks it against a static `unordered_set` of four hardcoded names. On a match — the node is skipped. Additionally, `extraExcludes_` is passed into the constructor (for non-standard PCH names, future config integration).

## Done

- Added the static method `LakosGodHeaders::isPchName()` with hardcoded: `pch.h`, `stdafx.h`, `precompiled.h`, `precompiled_header.h`
- Added the `extraExcludes` parameter to the constructor (for user exclusions without a YAML config)
- Filtering by `extraExcludes_` — uses `filename()`, not the full path (a PCH can live in any subdirectory)
- Fixture `fixtures/lakos_god_headers/pass/pch_excluded/`: `pch.h` + 31 `user*.h` with `#include "pch.h"` (fan-in = 31 > threshold 30)
- Two new test cases: all 4 PCH names, plus `extraExcludes` with the path `sub/my_pch.h`
- 193/193 tests green (`build+tests`)

## Key decisions

| Decision | Reason |
|----------|--------|
| Hardcode 4 names in a static `unordered_set` | Covers 95% of real projects without a config |
| Filter by `filename()`, not by path | A PCH can live in any subdirectory of the tree |
| `extraExcludes` in the constructor, not in YAML | The YAML config infrastructure does not exist yet; the parameter is already testable and prepared for future integration |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/rules/lakos_god_headers.h` | + `isPchName()`, + `extraExcludes_`, + `<unordered_set>` |
| `src/rules/lakos_god_headers.cpp` | PCH filtering implementation |
| `tests/unit/rules/lakos_god_headers_test.cpp` | +2 tests (known PCH names, extra excludes) |
| `fixtures/lakos_god_headers/pass/pch_excluded/` | 32 new fixture files |
