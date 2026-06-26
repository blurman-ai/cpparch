# [RULES] SF.8: kScanLines too small + .inc files should not be checked

**Created:** 2026-05-28
**Started:** —
**Status:** completed
**Module:** RULES
**Priority:** critical
**Difficulty:** S
**Blocks:** —
**Blocked by:** —
**Related:** #028 (rules_engine_mvp)

## Goal

Eliminate two SF.8 bugs that turn all of abseil-cpp (85 headers) into false positives.

## Context

A run on abseil-cpp (commit `e7a10c8`) produced 85 SF.8 violations — all false. Two independent bugs:

### Bug 1 — kScanLines = 30 too small

Abseil uses an Apache 2.0 license header before the include guard:

```cpp
// Copyright 2017 The Abseil Authors.
// Licensed under the Apache License, Version 2.0 ...
// ... 16 lines of license + empty lines ...
// (47 non-empty lines in total before #ifndef)
#ifndef ABSL_BASE_CONFIG_H_    ← line 48, beyond the limit!
#define ABSL_BASE_CONFIG_H_
```

Our limit `kScanLines = 30` in `sf8_include_guard.cpp`. The guard really is there — we just don't see it. Any project with an Apache/MIT copyright (most OSS) has the same problem.

**Fix:** raise `kScanLines` to 60 (covers all real copyright blocks, does not noticeably slow scanning).

### Bug 2 — .inc files are checked as headers

`src/scan/project_files.cpp` line 20: `".inc"` is in the list of header extensions and is checked by SF.8. But `.inc` files (e.g. `spinlock_linux.inc`, `spinlock_posix.inc` in abseil) are platform fragments without a guard by design: they are included exactly once via `#if PLATFORM ... #include "spinlock_linux.inc" #endif`. A guard in them would contradict their purpose.

**Fix:** remove `.inc` from the list of extensions checked by SF.8 (keep it in `isHeaderFile` for scanning the include graph — they participate in it legitimately).

## Execution plan

- [ ] `sf8_include_guard.cpp`: raise `kScanLines` from 30 to 60
- [ ] `sf8_include_guard.cpp`: add a check — skip files with the `.inc` extension
- [ ] Confirm: run on abseil → 0 SF.8 false positives
- [ ] Confirm: run on Catch2, fmt, spdlog does not regress
- [ ] Unit test: a header with an Apache 2.0 copyright (>30 non-empty lines) + a correct guard → pass

## Done

- `kScanLines` 30 → 60: covers Apache 2.0 copyright blocks (~47 non-empty lines)
- `isIncFile()` helper + `if (isIncFile(path)) continue;` in `check()` — `.inc` are skipped by SF.8, but stay in the include graph
- 2 new unit tests + fixture `fixtures/sf8_include_guard/pass/long_copyright_guard.h`
- Commit: `d9b74c2` `fix(rules/sf8): raise kScanLines to 60 and skip .inc fragments (#034)`

## Changed files

| File | Change |
|------|-----------|
| `src/rules/sf8_include_guard.cpp` | `kScanLines` 30→60; `isIncFile()` + skip in `check()` |
| `tests/unit/rules/sf8_include_guard_test.cpp` | 2 new tests: long-copyright guard, .inc fragment |
| `fixtures/sf8_include_guard/pass/long_copyright_guard.h` | new — Apache 2.0 copyright + guard on line 32 |
