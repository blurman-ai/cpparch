# [SCAN] #056 --diff: vendor-exclude (miniz/stb etc.)

**Created:** 2026-05-31
**Status:** dropped (absorbed by #065/#069/#070)
**Module:** SCAN
**Priority:** minor
**Related:** #060, #056

## Goal
Part of the "TP" in iter1 are vendored copies of libraries (two copies of `miniz`, `stb_image`):
technically copy-paste, but not project code. They should be cut by excludes (like a snapshot),
case-insensitively: `third_party`, `extern`, `vendor`, `borealis/.../extern`, and by
signals (license header / `@generated`).

## Verification
- [ ] iter-N: vendored pairs are not reported; real project copies remain.

## Outcome

**Status:** dropped — the goal was achieved by other tasks, no work of its own remained.
**Closed:** 2026-06-11 (backlog review).

The requested behavior was implemented three times at different levels:
- in the spike — vendor-exclude went into the iter2 fixes of #060 (commit `0beb2f7`, together with #061);
- in the product — vendor/third_party/extern exclusion centralized in #069
  (`src/scan/project_files.cpp`), the duplication pass doesn't see vendored files at all;
- generated files (`@generated`, protobuf/moc/flex-bison) are cut by the P0.9 guard `isGeneratedPath`
  (#065 → #070, `src/scan/duplication/duplication_scanner.cpp`), whole-file vendored-twin
  is suppressed by P0.2 (#070).

The only part of the original idea left unclosed is the "license header" signal — deliberately not done:
on the corpus, FPs of this class are already suppressed by paths and `@generated` markers.
