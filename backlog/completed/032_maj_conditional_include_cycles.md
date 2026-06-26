# [SCAN][RULES] Conditional includes: do not report cycles from #ifdef-guarded edges

**Created:** 2026-05-28
**Started:** 2026-05-28
**Status:** completed
**Module:** SCAN / RULES
**Priority:** major
**Difficulty:** M
**Blocks:** —
**Blocked by:** —
**Related:** #028 (rules_engine_mvp), #031 (godheader_pch_exclusion)

## Goal

The scanner must mark `#include` edges inside `#ifdef`/`#if` blocks as conditional, so that SF.9 does not report cycles that exist only in one of the build modes.

## Context

A run on spdlog (commit `2e71fdf`) produced 22 SF.9 violations. All 22 are `foo-inl.h ↔ foo.h` pairs, guarded by the macro `SPDLOG_HEADER_ONLY`:

```cpp
// foo.h, last lines:
#ifdef SPDLOG_HEADER_ONLY
#include "foo-inl.h"
#endif
```

```cpp
// foo-inl.h, first line:
#include <spdlog/foo.h>
```

In compiled mode (the default) this include is absent — there is no cycle. In header-only mode it is present intentionally: this is the classic dual-mode pattern of C++ libraries (spdlog, Abseil, part of Boost).

The text scanner does not expand `#ifdef` → all 22 edges enter the graph as ordinary ones → SF.9 reports legitimately by the letter, but falsely by the meaning.

**This is a typical false positive on real OSS projects.** Without a fix the tool will encounter these violations in spdlog, Abseil and other well-known libraries, which undermines trust in the results.

## Solution (chosen approach)

Mark conditional edges in the graph with a `conditional: true` flag.  
SF.9 (and other cycle rules) ignore cycles all of whose edges are conditional.

### Implementation steps

- [ ] In `IncludeToken` / the graph edge add a `conditional: bool` field
- [ ] In `include_scanner` track `#ifdef`/`#if`/`#ifndef` depth — mark any include inside as `conditional = true`
- [ ] In `DependencyGraph` store the flag on the edge (or a separate `conditional_edges` set)
- [ ] In SF.9 (`cycle_rule`): when traversing the SCC, skip cycles where all edges are `conditional`
- [ ] Fixtures: `fixtures/sf9/pass_conditional_ifdef/` — a `foo.h` + `foo-inl.h` pair via `#ifdef`
- [ ] Test: confirm that spdlog produces 0 SF.9 violations after the fix

## Alternatives (rejected)

**Option 1 — exclude pattern in config** (`exclude_cycles: ["**/*-inl.h"]`):
- Simpler to implement; the user controls it explicitly.
- Downside: requires config — breaks zero-config; does not protect against the same patterns with different naming.

**Option 3 — built-in `-inl.h` heuristic**:
- If `foo.h` includes `foo-inl.h` and vice versa — treat as a known pattern and do not report.
- Downside: hardcoded heuristic, hides real accidental cycles with `-inl.h` in the name; fragile.

## Done

- **2026-05-28** — Manual verification of the pattern on spdlog commit `2e71fdf`. Sources checked: `logger.h:385`, `common.h:373`, `base_sink.h:50`, `registry.h:130`, etc. All 22 pairs confirmed — each `.h` includes `*-inl.h` strictly inside `#ifdef SPDLOG_HEADER_ONLY`. The false positives are reproducible, the mechanics understood.
- **2026-05-28** — Full implementation: commit `04b523b` "suppress SF.9 on all-conditional include cycles (#032)". The scanner tracks `#if/#ifdef/#ifndef/#endif` depth (with include-guard recognition, so it does not mark the whole file conditional); `IncludeDirective::conditional`; `DependencyGraph` stores the flag on the edge (`unordered_set<uint64_t>`, a repeated unconditional edge wins); SF.9 skips cycles where all edges are conditional (`allEdgesConditional`). Tests 198/198. The task file meanwhile was not updated and sat in `new/` for 2 weeks — the TASK_TRACKER counted it as a P0 MVP blocker.

## How it works

An include inside any unclosed `#if` block (except the file's own include-guard) gets
`conditional = true`; the flag carries through to the graph edge. SF.9, when traversing an SCC, checks the found cycle:
if **all** its edges are conditional — the cycle exists only in one of the build modes (the dual-mode
spdlog/Abseil pattern) and is not reported. A mixed cycle (at least one unconditional edge) is reported
as before.

## Outcome

**Status:** completed
**Completed:** 2026-05-28 (actual, commit `04b523b`); the file was closed on 2026-06-11 following the backlog review.
**Open tail (handed to #088):** the fixture `fixtures/sf9/pass_conditional_ifdef/` was not created (coverage — by unit tests of the scanner and SF.9) and the control re-run of spdlog was not recorded — added to the final item of #088 "re-run the FP repos".
**Adjacent:** the `.inl` idiom without `#ifdef` (legate/acts) — this is a separate mechanism `isInlineSplitScc`, implemented in #088 §2.2 (`8c05878`); it does not overlap with the conditional filter.

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/scan/include_token.h` | `conditional` field |
| `src/scan/include_scanner.cpp` | `#ifdef` depth tracking |
| `include/archcheck/graph/dependency_graph.h` | conditional flag on the edge |
| `src/rules/sf9_no_cycles.cpp` | skip all-conditional cycles |
| `fixtures/sf9/pass_conditional_ifdef/` | new fixture |
| `tests/unit/rules/sf9_test.cpp` | conditional-cycle test |
