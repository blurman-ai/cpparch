# [SCAN] Test filter: hyphenated suffixes (-test) and testutil/ directories

**Created:** 2026-06-12
**Start date:** 2026-06-12
**Status:** done
**Module:** SCAN
**Priority:** minor
**Complexity:** small
**Assignee:** Haiku
**Blocks:** —
**Blocked by:** —
**Related:** #111 (finding §3.2 of the corpus run report), #070 (original test exclusion)

## Goal

Close two holes in test classification found by the corpus run #111
(apache/impala): basename `otel-test.cc` (hyphenated suffix) and the directory
`be/src/testutil/` pass the filter and land in the graph/metrics as
production code.

## Context

Facts verified live 2026-06-12 in
`include/archcheck/scan/file_classification.h`:

- `kTestDirNames` (~line 225): `{"test", "tests", "unittest", "unittests"}`;
  segments are normalized by `normalizeDirSegment` (lowercase, `_`/`-`/space
  stripped) — i.e. `unit_test/` is already caught as `unittest`.
- `isTestBasename` (~line 243): stem starts with `test_` OR ends with
  `_test` / `_tests` / `_spec` (array `kTestStemSuffixes`).
  **GCC8-COMPAT**: comparison via `compare()`, not `ends_with` — keep the style.
- Consumers (behavior changes for all of them, that's the point):
  `src/graph/graph_builder.cpp:60`, `src/scan/project_files.cpp:157`,
  `src/scan/god_file_growth.cpp:36,170`, `src/scan/defect_attractor.cpp:29`,
  `src/scan/test_co_evolution.cpp:42`.
- Tests: `tests/unit/scan/file_classification_test.cpp` —
  `pathHasTestDir` (line 142) and the neighboring `isTestBasename` TEST_CASE.

## Settled design (no forks)

1. `kTestDirNames` += `"testutil"`, `"testutils"` (segment normalization
   already collapses `test_util/`, `test-utils/` into these forms; IMPORTANT: the current
   normalization also collapses `test-utils` → `testutils` — do not add separate
   spellings to the array). Raise the array size in the declaration
   `std::array<std::string_view, N>` accordingly.
2. `isTestBasename`: to `kTestStemSuffixes` add `"-test"`, `"-tests"`,
   `"-spec"`; to the `test_` prefix check add `test-`.
   Do NOT add the bare suffix `test` without a separator — `contest.cc`,
   `attest.h`, `latest.h` must remain production.

## Execution plan

- [ ] Edit `file_classification.h` per the design (both items).
- [ ] Add cases to `tests/unit/scan/file_classification_test.cpp`
      (into the existing TEST_CASEs for `pathHasTestDir` / `isTestBasename`),
      do not change existing REQUIREs.
- [ ] Run the control cases, build, tests, lizard, dogfood.

## Control cases (contract)

| Call | Expectation |
|---|---|
| `isTestBasename("otel-test.cc")` | **true** |
| `isTestBasename("unit-tests.cpp")` | **true** |
| `isTestBasename("test-main.cc")` | **true** |
| `isTestBasename("widget-spec.cpp")` | **true** |
| `isTestBasename("contest.cc")` | **false** |
| `isTestBasename("attest.h")` | **false** |
| `isTestBasename("latest.h")` | **false** |
| `pathHasTestDir("be/src/testutil/scoped-flag-setter.h")` | **true** |
| `pathHasTestDir("be/src/test_utils/foo.h")` | **true** |
| `pathHasTestDir("be/src/observe/span-manager.cc")` | **false** |
| `pathHasTestDir("src/testutility/foo.h")` | **false** (segment ≠ testutil/testutils) |

## Do not do

- Do NOT change existing test expectations — only add new REQUIREs.
- Do NOT touch vendor classification (`kVendoredDirNames` etc.) — that is #113.
- Do NOT use `std::string_view::ends_with` — GCC8-COMPAT, see the comment
  in the function itself.
- Do NOT commit without a command.

## Definition of done

- 11/11 control cases green.
- `cmake --build build/debug` + `ctest --output-on-failure` — all green
  (including the untouched old cases of the classification test).
- `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — 0 warnings.
- Dogfood: `./build/debug/src/archcheck` from the root — 0 violations.

## Done

- `kTestDirNames`: expanded from 4 to 6 entries — added `"testutil"`, `"testutils"`.
  The `normalizeDirSegment` normalization automatically collapses `test_util/`, `test-utils/`
  into these forms, no separate spellings were needed.
- `isTestBasename`: added prefix `test-` (right after `test_`); `kTestStemSuffixes`
  expanded from 3 to 6 — added `"-test"`, `"-tests"`, `"-spec"`.
- GCC8-COMPAT: suffix comparison via `compare()`, `ends_with` not used.
- 11/11 control cases green, 31/31 assertions in the `[scan][test]` test cases.
- 506/506 tests, lizard 0 warnings, dogfood 0 violations.
- Coverage: lines 91.5% / functions 96.5% / branches 57.6% — PASS.
- Commit: `362ca60` (`fix(scan): add hyphenated test suffixes and testutil dirs (#114)`)

## In progress

- (empty)

## Next steps

- (empty)

## Key decisions

| Decision | Reason |
|---------|---------|
| Only separated suffixes (`-test`, not `*test`) | contest/attest/latest — FP class; the separator is mandatory |
| `testutil(s)` as a dir name, not a basename | the impala case — a directory; the basename `testutil.cc` without a suffix stays production |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/scan/file_classification.h` | `kTestDirNames` +2, `kTestStemSuffixes` +3, prefix `test-` (commit `362ca60`) |
| `tests/unit/scan/file_classification_test.cpp` | +11 control REQUIREs (commit `362ca60`) |

## How it works

`normalizeDirSegment` (lowercase + strip `_/-/space`) already collapses `test-utils/`
and `test_util/` into `testutils` / `testutil` — so only the normalized forms were
added to the array. The suffixes `-test`/`-tests`/`-spec` are caught by the same
`compare()` loop as `_test`/`_tests`/`_spec` — GCC8 compatibility preserved.
The bare suffix `test` is deliberately not added: it would catch `contest`, `attest`, `latest`.

## Completion date

2026-06-12
