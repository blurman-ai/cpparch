# [SCAN][RULES] SF.9 stays silent on projects with `#ifndef` guards: an include inside an include-guard is falsely conditional

**Created:** 2026-05-29
**Started:** 2026-05-29
**Completed:** 2026-05-29
**Status:** completed
**Module:** SCAN / RULES
**Priority:** critical
**Difficulty:** M
**Blocks:** v0.1 release
**Blocked by:** —
**Related:** #032 (conditional cycles suppression — source of the regression), #028 (rules_engine_mvp)

## Goal

SF.9 must report real cycles in projects with the standard include-guard `#ifndef X / #define X / ... / #endif`. Right now, on any such project SF.9 gives **0** even when a cycle is present, because the scanner treats the include-guard as a conditional context and `allEdgesConditional` mutes the SCC.

## Context

Re-run of the dogfood corpus on 2026-05-29 (see milestones):

| Project | Guard style | `sccs_cyclic` (--graph) | SF.9 | Discrepancy |
|---|---|---|---|---|
| fmt | `#pragma once` | 0 | 0 | — |
| spdlog | `#pragma once` | 22 | 0 | suppressed by #032 correctly (all cycles `foo.h ↔ foo-inl.h` under `#ifdef SPDLOG_HEADER_ONLY`) |
| Catch2 | `#pragma once` | 0 | 0 | — |
| nlohmann/json | `#pragma once` | 0 | 0 | — |
| abseil-cpp | `#pragma once` | 0 | 0 | — |
| folly | `#pragma once` | 9 | 9 | — |
| **grpc** | **`#ifndef`** | **4** | **0** | **critical** |

In grpc there are 4 real SCCs, among them a genuine architectural cycle in grpc itself:

```
src/core/tsi/alts/handshaker/alts_handshaker_client.h ↔ alts_tsi_handshaker.h
```

Both `#include`s are at the top level of the file, with no `#ifdef` nearby other than the guard wrapping the whole file:

```cpp
// alts_handshaker_client.h
#ifndef GRPC_SRC_CORE_TSI_ALTS_HANDSHAKER_ALTS_HANDSHAKER_CLIENT_H
#define GRPC_SRC_CORE_TSI_ALTS_HANDSHAKER_ALTS_HANDSHAKER_CLIENT_H
...
#include "src/core/tsi/alts/handshaker/alts_tsi_handshaker.h"  // ← marked conditional=YES
...
#endif
```

Diagnostics via `/tmp/scc_dump` (a one-off debug binary, source
`/tmp/scc_dump.cpp`) on grpc showed that **all 4 SCCs** contain edges with
`conditional=YES`. After that, `Sf9NoCycles::check` discards them via
`allEdgesConditional` and does not report.

## Why it was hidden

All the other projects in the corpus use `#pragma once` — in them no `#ifndef` block is opened, and the scanner doesn't mark includes as conditional. grpc is the first project in the sample with the classic guard style, so the bug only surfaced now.

In the existing SF.9 / scanner unit tests, `#pragma once` or simplified test headers are used → the bug isn't caught through them.

## Scale

This is **critical**: archcheck is positioned as a cycle-catcher in CI. On any project with `#ifndef` guards (most OSS C/C++ code: grpc, LLVM, Linux-style, classic libraries) SF.9 is currently useless. This must be closed before the v0.1 release.

## Solution

In the `include_scanner`, recognize the standard include-guard pattern:
- the first non-comment / non-empty directive in the file is `#ifndef X`
- immediately followed by `#define X`
- where X is an un-shouty identifier (by convention `[A-Z_][A-Z0-9_]*`)

When this pattern is detected — don't count the first `#ifndef` in the `#if` depth counter, and mirror this by ignoring the matching closing `#endif` at the end of the file. Then the file content is treated as "top level", and includes are not marked conditional except those genuinely nested in `#ifdef`/`#if`.

Alternative: weaken #032 (e.g. consider a cycle all-conditional only if ALL its include edges are under `#ifdef X` or `#if defined(X)` directives with the same X) — more complex, and it doesn't cover the case where the guard surrounds a genuinely conditional include.

## Plan

- [ ] Fixture `fixtures/sf9_no_cycles/ifndef_guard_real_cycle/` — two `.h` files with a classic `#ifndef` guard forming a cycle. Expected behavior: SF.9 reports.
- [ ] Fixture `fixtures/sf9_no_cycles/ifndef_guard_no_cycle/` — the same two files, but no cyclic include. Expected behavior: SF.9 stays silent, no false positive.
- [ ] Unit test on the scanner: `IncludeScanner` for a file with an include-guard — all top-level includes must have `conditional=false`.
- [ ] Implement include-guard recognition in the scanner (a new `detectIncludeGuard()` function or similar).
- [ ] Re-run on grpc — 4 SF.9 violations should surface (or however many there really are after filtering out non-ours cycles).
- [ ] Check for regressions on the rest of the corpus (folly should stay 9, spdlog — 0, the rest — 0).
- [ ] Update milestones (§"Run 9 — grpc": remove "requires investigation", add a link to this task and the fix outcome).
- [ ] Update STATUS.md — remove "one unresolved mismatch" from the header.

## Changed files

All in commit `595bdf2`:

| File | Change |
|------|--------|
| `src/scan/include_scanner.cpp` | `pp_argument()`, `is_shouty_ident()`, `is_blank_line()`, `next_significant_line()`, `detect_include_guard_offset()`; the main loop skips `++depth` for the guard opening |
| `tests/unit/scan/include_scanner_test.cpp` | +5 tests tagged `[guard]` |
| `fixtures/sf9_no_cycles/fail_ifndef_guard_real_cycle/{alpha,beta}.h` | new — two `#ifndef`-guarded files with a cycle |
| `fixtures/sf9_no_cycles/pass_ifndef_guard_no_cycle/{alpha,beta}.h` | new — two guarded files without a cycle |
| `docs/milestones.md` | §"Run 9 — grpc": marked Resolved 2026-05-29 with a summary |
| `docs/STATUS.md` | mismatch removed from the header and from "Open bugs" |

An SF.9-level test (`tests/unit/rules/sf9_no_cycles_test.cpp`) was not added: the rule works at the `DependencyGraph` level and is already covered (including the `allEdgesConditional` case). The regression is guarded by the scanner tests and the fixture pair.

## Done

- Fixtures `fixtures/sf9_no_cycles/fail_ifndef_guard_real_cycle/` and `pass_ifndef_guard_no_cycle/`.
- 5 unit tests in [tests/unit/scan/include_scanner_test.cpp](../../tests/unit/scan/include_scanner_test.cpp) tagged `[guard]`: the positive case, a case with leading comments, a nested `#ifdef` inside the guard, and two negative ones (no `#define` / mismatched ident).
- Implemented `pp_argument()`, `is_shouty_ident()`, `detect_include_guard_offset()` in [src/scan/include_scanner.cpp](../../src/scan/include_scanner.cpp); the main loop skips `++depth` on the guard's `#ifndef`.
- ctest: **235/235 passed**, lizard --warnings_only: clean.
- Corpus re-run: **grpc SF.9 = 4** (including the real architectural cycle `alts_handshaker_client.h ↔ alts_tsi_handshaker.h`), folly = 9 (unchanged), spdlog = 0 (correct suppression per #032), fmt/Catch2 = 0.
- Updated `docs/milestones.md` §"Run 9 — grpc" (Resolved 2026-05-29) and `docs/STATUS.md` (mismatch removed from the header and from "Open bugs").

## How it works (principle)

**Problem.** `include_scanner` marked `IncludeDirective.conditional = true` for all directives inside any open `#if`/`#ifdef`/`#ifndef` block — including the classic include-guard `#ifndef X / #define X / ... / #endif`, which wraps the whole file. After #032, the SF.9 rule filtered SCCs via `allEdgesConditional` and suppressed real cycles in any project with such guards.

**Solution.** Before the main pass, the scanner searches once for the "file header":

1. The first non-empty line is `#ifndef X`, where `X` is an ALL_CAPS identifier (`[A-Z_][A-Z0-9_]*`).
2. The immediately following non-empty line is `#define X` with the same `X`.

If the header is recognized, the offset of the `#ifndef` line is saved. In the main loop this one line does not increment `depth`. The matching closing `#endif` then ends up at position `depth == 0`, and the existing `depth > 0` guard silently ignores it — no separate close-handling logic is required.

**What didn't break.** Comments and blank lines are already turned into whitespace in `preprocess()`, so the "first non-blank line" detection automatically skips license headers. Nested `#ifdef OPT` inside the guard works as before — depth there starts at 0, increments on `#ifdef`, and includes inside get `conditional=true`. `#pragma once` files are rejected by the detector at the very first step (the first directive isn't `#ifndef`), and their behavior is unchanged.

**Application boundary.** The heuristic is conservative — it requires an ALL_CAPS ident and an immediately following `#define`. This is deliberate: we catch the standard pattern (LLVM, grpc, Boost, classic libraries) and don't try to embrace nonstandard guards with `#if !defined()` / lowercase / a deferred `#define`. Such projects can be added to the heuristic separately if needed.

## How it works

<!-- TODO: canonicalize the section from "How it works (principle)" above -->

## What controls it

<!-- TODO -->

## What it relates to

<!-- TODO -->

## Diagnostics

<!-- TODO -->
