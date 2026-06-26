# [RULES] GodHeader: structural library headers must not be reported as violations

**Created:** 2026-05-28
**Started:** 2026-05-29
**Completed:** 2026-05-29
**Status:** completed
**Module:** RULES
**Priority:** minor
**Complexity:** S
**Blocks:** —
**Blocked by:** —
**Related:** #028 (rules_engine_mvp), #031 (godheader_pch_exclusion)

## Goal

Reduce GodHeader noise on large libraries where high fan-in is an architectural norm, not a defect.

## Context

Runs on OSS projects revealed a category of "structural god-headers":

| File | Fan-in | Nature |
|------|--------|--------|
| `absl/base/config.h` | **507** | Fundamental config for all of Abseil. 508 direct includes. Effectively `<stddef.h>` |
| `absl/base/attributes.h` | 191 | Compiler attribute macros — needed everywhere |
| `catch_test_macros.hpp` | 108 | Main macro header of the test framework — must be in every test |
| `doctest_compatibility.h` | 77 | Test-compat shim — same idea |
| `spdlog/common.h` | 38 | Base spdlog types — a possible real candidate |

The threshold 30 is right for user code, but too small for "platform headers" and "test macro headers" in libraries.

## Solution

**Option A — raise the default threshold 30 → 50.**

The simplest fix, requires no config. Removes noise on abseil (most god-headers
have fan-in 50+, the remaining ones are genuinely interesting). Does not silence `spdlog/common.h` (fan-in 38 < 50)
— it stays in the report as a real candidate for splitting.

Options B (config setting) and C (exclude patterns) are deferred until the
YAML config appears (post-v0.1).

## Done

- [x] `include/archcheck/rules/lakos_god_headers.h`: `kDefaultThreshold = 50`.
- [x] `tests/unit/rules/lakos_god_headers_test.cpp`: default test updated to 50.
- [x] `docs/architecture-spec.md`: edits in §"Lakos. Rules" and §"MVP v0.1".
- [x] `docs/STATUS.md`: God-headers checklist line updated + removed from the candidate list.

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/rules/lakos_god_headers.h` | `kDefaultThreshold` 30 → 50 |
| `tests/unit/rules/lakos_god_headers_test.cpp` | default check 50 instead of 30 |
| `docs/architecture-spec.md` | two mentions of "default 30" → "default 50" |
| `docs/STATUS.md` | checklist + candidates |

## How it works

The GodHeader rule (`LakosGodHeaders`) counts the in-degree of each header
(how many other files `#include` it) and reports a violation if the value
exceeds `threshold_`. The constructor accepts the threshold as an optional parameter
(`LakosGodHeaders rule(my_threshold)`), while `kDefaultThreshold` sets the value
for `LakosGodHeaders rule;` — this same constructor is called from
`rule_set.cpp:20` for the default set.

Replacing 30 → 50:
- Calibration against real libraries: on abseil the fundamental headers
  (`absl/base/config.h` fan-in 507, `attributes.h` 191) are obviously above any
  reasonable threshold — they are noise at any value ≤ ~150. But 30 additionally
  caught "medium" utility headers (~30-50 fan-in) which are normal in library
  code.
- 50 is a compromise: it cuts noise on `catch_test_macros.hpp` (108) — that one stays,
  but `spdlog/common.h` (38) also stops being a god-header, which is a deliberate
  loss (it was already marked in milestones as a "possible candidate", not a sure one).
- When config arrives, the threshold will become configurable via
  `rules.lakos_god_header.threshold` (Option B in the original plan).

The tests in `lakos_god_headers_test.cpp` all use an explicit threshold (`LakosGodHeaders(5)`),
except one case that checks exactly the default — that one was updated.
