# [RULES][SF] Reduce the number of findCycleImpl parameters

**Creation date:** 2026-05-28
**Start date:** 2026-05-28
**Status:** done
**Module:** RULES
**Priority:** minor
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** —

## Goal

Eliminate the lizard-threshold violation (≤5 parameters) in `findCycleImpl`, without changing the algorithm.

## Context

`findCycleImpl` in `src/rules/sf9_no_cycles.cpp` — an internal DFS for finding a cycle
within a single SCC had 7 parameters at the lizard threshold of ≤5.

### Why it arose

The function is written as a free recursive DFS carrying all the mutable state
through the call stack. `bool started` is a crutch to avoid stopping at the start
vertex immediately (`cur == target`). In C++ this is naturally solved via a `struct`
with fields — the state lives in fields, not parameters.

## Done

- [x] Introduce `struct CycleFinder` in an anonymous namespace
- [x] Move `findCycleImpl` into `CycleFinder::dfs(NodeId cur)` — 1 parameter
- [x] Remove `bool started`; the start call goes over the neighbors of `start`
- [x] lizard: 0 violations
- [x] Tests: 11 assertions in 4 test cases — all passed

## Changed files

| File | Change |
|------|-----------|
| `src/rules/sf9_no_cycles.cpp` | `findCycleImpl` → `CycleFinder::dfs` |

---

**Completion date:** 2026-05-28

## How it works

`CycleFinder` — a struct in an anonymous namespace. Const fields (`g`, `members`,
`target`) are initialized at creation. The mutable state (`vis`, `path`) —
fields, accumulated in `dfs()`. `check()` creates a `CycleFinder` per SCC
and runs `finder.dfs(next)` over the neighbors of the start vertex — this replaces
`bool started` without extra logic.

## What controls it

Nothing — an internal implementation detail of `Sf9NoCycles::check()`.

## What it's connected to

`src/rules/sf9_no_cycles.cpp`, `include/archcheck/rules/sf9_no_cycles.h`,
`tests/unit/rules/sf9_no_cycles_test.cpp`.

## Diagnostics

On a regression: `lizard --arguments 5 --warnings_only src/rules/sf9_no_cycles.cpp`
should return 0 lines of output.
