# [REPORT][DOGFOOD] Found with our own tool: extract `jsonEscape` into a shared helper

**Created:** 2026-05-30
**Started:** 2026-05-30
**Completed:** 2026-05-30
**Status:** completed
**Module:** REPORT
**Priority:** minor
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** #053 (fast_backend_line_duplication_pass — the line-dup spike that found the duplicate), #051 (config_loader_v1 — baseline is already in the product, so the dedup is needed in shipped code)

## Goal

Remove a real internal copy-paste in `archcheck::report`: an identical
`jsonEscape(const std::string&)` currently lives both in
[src/report/json_reporter.cpp](../../src/report/json_reporter.cpp) and in
[src/report/violation_baseline.cpp](../../src/report/violation_baseline.cpp).
Extract it into a single shared helper and use this as the first clean
**dogfooding story** for the duplicate detector: "found with our own tool, fixed".

## Story of the find

During a run of the standalone line-dup spike over `cpparch` itself, the top
cross-file block for the repository turned out to be this:

- `src/report/json_reporter.cpp:6`
- `src/report/violation_baseline.cpp:12`
- length `21` significant lines
- similarity `100%`

A manual check confirmed it's not a test twin, not generated code, and not a
packaging artifact. It's a **real hand-written duplicate** in the same module
`archcheck::report`: two files holding the same minimal JSON escaping
for `"`, `\`, and `\n`.

This is a good teaching case for the product itself:

1. the fast line-dup layer found a non-trivial duplicate in its own code;
2. the find is small enough to fix right away;
3. after the fix we can honestly say `archcheck` started with itself.

## Context

Right now the code looks like this:

- [json_reporter.cpp](../../src/report/json_reporter.cpp) contains a local
  `jsonEscape()` in an anonymous namespace and uses it when serializing the report;
- [violation_baseline.cpp](../../src/report/violation_baseline.cpp) contains
  a second local `jsonEscape()` with the same body and uses it when saving the
  baseline.

`jsonUnescape()` and baseline parsing, meanwhile, are needed only by
`violation_baseline.cpp`; there's no need to drag them into the shared util.

## What to do

- [x] Extract the shared helper `jsonEscape()` into a small report-level util.
- [x] Switch `json_reporter.cpp` and `violation_baseline.cpp` to one shared implementation.
- [x] Keep `jsonUnescape()` and the baseline-specific parsing local, if
      they have no second user.
- [x] Add a test for the helper's behavior: at least `"`, `\`, `\n`.
- [x] After the fix, re-run the line-dup spike on `cpparch` and confirm that this
      specific block no longer surfaces as a top cross-file duplicate.

## Constraints

- Don't make a "bag of utilities" like `json_utils.cpp` without need.
- Don't pull a general JSON refactor or a baseline/report format change into the task.
- Don't change serialization behavior; the task is only about deduplicating the implementation.

## Expected solution

The minimal variant:

- `include/archcheck/report/json_escape.h`
- `src/report/json_escape.cpp`

with one function:

```cpp
std::string jsonEscape(const std::string& s);
```

If during implementation it turns out the `.cpp` isn't needed and a
header-only helper can be kept without bloating include noise — that's also acceptable. The main thing:
**one implementation, two users, no extra abstraction.**

## Acceptance criteria

- [x] Exactly one implementation of `jsonEscape()` remains in the codebase.
- [x] `json_reporter` and `violation_baseline` use the same helper.
- [x] Behavior unchanged for `"`, `\`, `\n`.
- [x] There is a test that locks this in.
- [x] After the fix, the dogfooding case is documented (see "Result"): the pair
      `json_reporter ↔ violation_baseline` left the cross-file duplicates of `src/`.

## Key decisions

| Decision | Reason |
|----------|--------|
| Fix exactly `jsonEscape`, not the whole JSON layer | the task is small and confirmed by the spike |
| Don't extract `jsonUnescape` ahead of time | no second user, YAGNI |
| Use the case as a dogfooding story | it's a strong and honest narrative for the product |
| Don't make a generic `json_utils` | one helper doesn't justify a bag of abstractions |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/report/json_escape.h` | shared helper for JSON string escaping |
| `src/report/json_escape.cpp` | helper implementation (or a header-only alternative) |
| `src/report/json_reporter.cpp` | remove the local copy, include the helper |
| `src/report/violation_baseline.cpp` | remove the local copy, include the helper |
| `tests/unit/report/...` | a test for `jsonEscape()` and/or affected roundtrip |

## Next steps

1. Add the helper and switch the two call sites.
2. Add a test.
3. Re-run the spike on `cpparch` and save a short note in the completed task
   as a dogfooding story.

## Result (2026-05-30)

**Done.** `jsonEscape()` extracted into a **header-only** helper
[include/archcheck/report/json_escape.h](../../include/archcheck/report/json_escape.h)
(`inline`, the only dependency — `<string>`; a separate `.cpp` was not created —
for a tiny pure function it would be an extra TU + a CMake entry). Both call sites
([json_reporter.cpp](../../src/report/json_reporter.cpp),
[violation_baseline.cpp](../../src/report/violation_baseline.cpp)) switched to
it; `jsonUnescape()` and the baseline parsing stayed local in
`violation_baseline.cpp` (no second user → YAGNI). Test:
[tests/unit/report/json_escape_test.cpp](../../tests/unit/report/json_escape_test.cpp)
(`"`, `\`, `\n` + a combination). The Debug build is green, the whole suite — 255 cases /
812 assertions pass. Exactly one definition of `jsonEscape` (in the header).

**Dogfooding result.** The spike (`experiments/line_duplication`, the binary
`line_duplication`) was built and actually run over `src/`:

```
$ line_duplication src --cross-only --min-lines 6 --top 12
duplicated blocks: 7   cross-file blocks: 5
top duplicated blocks:
  [CROSS-FILE] 10 lines  rules/sf7_using_namespace.cpp:83  <->  rules/sf8_include_guard.cpp:79
  [CROSS-FILE]  7 lines  graph/algorithms.cpp:9           <->  graph/baseline.cpp:13
  [CROSS-FILE]  7 lines  rules/lakos_god_headers.cpp:20   <->  rules/sf7_using_namespace.cpp:84
  [CROSS-FILE]  7 lines  rules/sf7_using_namespace.cpp:3  <->  rules/sf8_include_guard.cpp:3
  [CROSS-FILE]  6 lines  scan/include_resolver.cpp:4      <->  scan/include_scanner.cpp:5
```

The pair **`json_reporter.cpp ↔ violation_baseline.cpp` is completely gone** from
the cross-file duplicates of `src/` — the original 21-line `jsonEscape` block disappeared, and
the spike no longer shows any residual block between these two files.
Criterion met.

**First clean dogfooding story:** the fast line-dup layer found a real hand-written
duplicate in its own code → fixed → the re-run confirmed it disappeared. "Started
with ourselves".

**Side observation (out of scope for #055).** Other internal pairs now surface in the
top (sf7/sf8, graph algorithms/baseline, etc.) — these are mostly rule-header
boilerplate and include blocks, not necessarily real copy-paste.
I'm not opening a separate task without a command.

## How it works

`jsonEscape()` lives as a single `inline` function in
`include/archcheck/report/json_escape.h` (the dependency — only `<string>`).
Both report writers (`writeJsonReport`, `saveBaseline`) include this header and
call the shared helper instead of local copies. Behavior is byte-for-byte the same:
`"`, `\`, `\n` are escaped.

## Key decisions

- **Header-only `inline`, no `.cpp`** — a tiny pure function; a separate TU
  and a CMake line would be extra weight.
- **`jsonUnescape()` not extracted** — the only user is
  (`violation_baseline.cpp`); extracting it "for the future" = violating YAGNI.
- **The duplicate was found by our own line-dup spike** — used as the first honest
  dogfooding story, not just a refactor.

## Changed files

- `include/archcheck/report/json_escape.h` — new header-only helper `jsonEscape()` (commit b3c9594)
- `src/report/json_reporter.cpp` — removed the local copy, included the helper (b3c9594)
- `src/report/violation_baseline.cpp` — removed the local copy, included the helper (b3c9594)
- `tests/unit/report/json_escape_test.cpp` — new test for the helper (b3c9594)
- `tests/CMakeLists.txt` — test registration (b3c9594)

## Controlled by

<!-- TODO -->

## Related to

<!-- TODO -->

## Diagnostics

<!-- TODO -->

## Result

**Status:** completed
**Completed:** 2026-05-30
**Commit:** b3c9594
