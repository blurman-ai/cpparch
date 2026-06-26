# [SCAN][GRAPH] A project's license banner ≠ vendor: remove the over-exclusion in the graph

**Created:** 2026-06-12
**Started:** 2026-06-12
**Status:** done
**Module:** SCAN][GRAPH
**Priority:** major
**Complexity:** small
**Assignee:** Haiku
**Blocks:** — (softly: the future rule DRIFT.4)
**Blocked by:** —
**Related:** #111 (the §5.2 finding of the corpus run report), #081 (a previous over-exclusion: SPDX)

## Goal

The graph must not lose a project's own files just because the project honestly
puts a full license banner (Apache/MIT/BSD) in each of its files.
Right now FDio/vpp (Apache-2.0, full banner in every file before the SPDX migration)
yields a graph of **267 nodes out of 2621 files** — 90% of the project thrown out as "vendor".

## Context

The #111 corpus run (see
[docs/research/lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md) §5.2)
uncovered: `hasVendorLicenseHeader()` treats the full text of the Apache banner
(marker `"licensed under the apache"`) as a signal of a vendored file. The heuristic
was written for the case "a foreign file with a foreign license among ours", but for
Apache-licensed projects the banner sits in EVERY one of their own files.

Facts verified live on 2026-06-12:

- `include/archcheck/scan/file_classification.h` — `hasVendorLicenseHeader()`
  (~line 190, the array `kLicenseMarkers` of 6 markers),
  `isVendoredFile()` (~line 215) = name-layer ∨ license-layer.
- `src/graph/graph_builder.cpp` — `filterVendored()` (lines 55–74):
  the single place where the license layer affects the **graph** (via
  `isVendoredFile(baseName, src)` on line 66). It already reads the content of all
  files into `FilteredFiles::contents`.
- Other consumers of `isVendoredFile`: `src/scan/project_files.cpp:162`
  (the duplication pipeline), `src/scan/test_co_evolution.cpp:38` (passes `""` —
  the license layer is effectively off). Their behavior is NOT to change — out of scope.
- Tests for the per-file function: `tests/unit/scan/file_classification_test.cpp:53–78` —
  stay green without edits (we don't change the function itself).
- There are no tests for `graph_builder.cpp`; in-memory `FileSource` stubs as samples:
  `FakeSource` in `tests/unit/scan/project_files_test.cpp:256`,
  `MapFileSource` in `tests/unit/scan/local_complexity_drift_test.cpp:13`.

## Decided design (no forks)

Fix in `filterVendored()` (graph_builder.cpp), NOT in `file_classification.h`:

1. First pass: for each file that passed the dir/name filters
   (`pathHasVendoredDir` / `pathHasTestDir` / `isTestBasename` /
   `isVendoredBasename` — leave the name layer as is), read the content and
   compute `hasVendorLicenseHeader(src)`.
2. If the banner is on **> 50%** of such files — it's the project's license: the license layer
   is **disabled** for this run (don't throw out anyone by the banner).
   Otherwise — the current behavior (files with the banner are thrown out).
3. `isVendoredBasename` (qcustomplot/stb_/imgui etc.) applies always,
   regardless of the share — it's the name layer.

Implementation note: `filterVendored()` already reads all contents — collect
triples (file, content, banner-flag) into a single vector in the first pass, decide by
the share, filter out in a second pass. Don't add new disk reads.

## Execution plan

- [ ] Rebuild `filterVendored()` per the design above (two-pass, same
      signature, ≤ 30 lines per function — extract a helper into the same file
      if needed).
- [ ] A new test file `tests/unit/graph/graph_builder_test.cpp` (+ a line in
      `tests/CMakeLists.txt` — look at how the neighboring
      `tests/unit/graph/*.cpp` are wired). Copy the stub FileSource following the
      `FakeSource` sample (project_files_test.cpp:256).
- [ ] Run the control cases, build, tests, lizard, dogfood.

## Control cases (the contract)

Start all contents in the tests with the banner as the FIRST bytes (the function looks at
the first 2000 bytes).

| Case | Input | Expectation |
|---|---|---|
| 1. Project license | 4 files `a.h b.h c.cpp d.cpp`, ALL with `// Licensed under the Apache License, Version 2.0`, `d.cpp` includes `"a.h"` | nodes = **4**, edges = **1** |
| 2. Real vendor | 5 files: 4 without a banner, 1 (`mini_lib.h`) with `/* Permission is hereby granted, free of charge */`; one of the clean ones includes `"mini_lib.h"` | nodes = **4**, edges = **0**, external+unresolved = 1 |
| 3. Exactly 50% | 4 files, banner on 2 | share NOT > 0.5 → banner ones thrown out: nodes = **2** |
| 4. Name-layer under dominance | 4 files with an Apache banner, one of them `qcustomplot.cpp` | nodes = **3** (qcustomplot thrown out by name, the banner layer is off) |

## Do not do

- Do NOT change `hasVendorLicenseHeader` / `isVendoredFile` / `kLicenseMarkers`
  in `file_classification.h` — the per-file semantics stays.
- Do NOT change the existing expectations in `file_classification_test.cpp`.
- Do NOT touch `project_files.cpp` / `test_co_evolution.cpp` (the duplication path
  is deliberately left with the old behavior — a separate decision, not a gap).
- Do NOT add a config knob/flag for the 50% threshold — a hardcoded constant
  with a comment explaining the reason.
- Do NOT commit without a command.

## Definition of done

- 4/4 control cases green with the exact numbers from the table.
- `cmake --build build/debug` + `ctest --output-on-failure` — all green.
- `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — 0 warnings.
- Dogfood: `./build/debug/src/archcheck` from the root — 0 violations on src/include/tests.

## Done

- `filterVendored()` rewritten as two-pass. Added `struct FilterEntry`
  (file, content, hasBanner) in an anonymous namespace; `std::count_if` via
  `<algorithm>` (include added).
- First pass: excludes by the dir/test/basename filters, for the rest reads
  the content and records the banner flag into the candidates vector.
- Dominance: `nBanner * 2 > candidates.size()` — strictly > 50%.
- Second pass: under dominance all candidates pass; otherwise the banner
  files are excluded (old behavior). `isVendoredBasename` (name layer)
  works always, regardless of the share.
- New file `tests/unit/graph/graph_builder_test.cpp`: 4 control cases,
  17 assertions; registered in `tests/CMakeLists.txt`.
- 4/4 control cases from the table green (nodeCount and edges exactly).
- 506/506 tests, lizard 0 warnings, dogfood 0 violations.
- Coverage: lines 91.5% / functions 96.5% / branches 57.6% — PASS.
- Commit: `68437c0` (`fix(graph): project Apache banner ≠ vendor; two-pass filterVendored (#113)`)

## In progress

- (empty)

## Next steps

- (empty)

## Key decisions

| Decision | Rationale |
|---------|-----------|
| Banner dominance (>50%) = the project's license | Config-free, catches the whole VPP class; pinpoint markers don't scale |
| Fix in graph_builder, not in file_classification | The per-file function is correct for its own question; only the project-wide interpretation is wrong |
| Don't touch the duplication path | It was calibrated separately (#053–#059); mixing semantics in one change is not allowed |

## Changed files

| File | Change |
|------|--------|
| `src/graph/graph_builder.cpp` | `filterVendored()` → two-pass with a dominance threshold (commit `68437c0`) |
| `tests/unit/graph/graph_builder_test.cpp` | new: 4 control cases (commit `68437c0`) |
| `tests/CMakeLists.txt` | + the new test file (commit `68437c0`) |

## How it works

The first pass collects a vector `FilterEntry{file, content, hasBanner}` for all files
that passed the dir/test/basename filters. Then the share is computed: if `nBanner * 2 >
candidates.size()` (strictly > 50%) — the banner is the project's own license, the banner layer
is turned off. Otherwise — the old behavior (banner files = vendor). `isVendoredBasename`
(qcustomplot, stb_, imgui, etc.) is applied before counting banners and doesn't depend on the share.
`hasVendorLicenseHeader` / `isVendoredFile` in `file_classification.h` are untouched —
the per-file semantics is correct, only the project-wide interpretation was wrong.

## Completion date

2026-06-12
