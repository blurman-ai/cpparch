# [CHEAP-DRIFT][HISTORY] Defect Attractor Signal

**Created:** 2026-06-10
**Started:** 2026-06-11
**Status:** wip
**Module:** GIT / HISTORY / REPORT
**Priority:** minor
**Difficulty:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #048 (drift_clean_checkout_methodology), #075 (trusted_diff_workflow), #098 (god_file_growth)

## Goal

Identify files that disproportionately often end up in fix-like commits and therefore look like defect attractors.

This is report-only / advisory analytics for review prioritization. Not an architectural verdict and not a bug predictor "out of the box".

## Context

- Source of the task: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 8.
- The task is close to #098 in technique: a history pass over `git log --numstat`, but the classification is driven not by file growth but by the nature of commit messages.
- This is already beyond strict authority-backed default rules. So the signal can only live as a report/highlight.
- We must not slide into SZZ, blame chains, ownership maps and other heavy machinery. The original task explicitly requested a cheap signal.

## Execution plan

### Detection contract

- A commit is considered fix-like if the message matches the patterns `fix`, `bug`, `crash`, `regression`, `hotfix`, `segfault`, `assert`, etc.
- Specifics for v1:
  - regex (case-insensitive, over the subject line): `\b(fix(es|ed)?|bug|crash|regress(ion|ed)?|hotfix|segfault|fault|oops|leak)\b`;
    word-boundary cuts off `prefix|suffix|postfix|fixture` (locked by a test);
  - merge commits skipped; rename/format commits filtered by the heuristic "fix-like AND more than 30 files touched" (mechanical);
  - window: the last 200 commits (shared `history_query` with #098);
  - attractor: a file in the **top decile** by the number of fix-like touches with `fix_touches >= 5`;
- For each file we aggregate:
  - number of fix-like commits in the window;
  - position relative to percentile/top-N;
- Finding:
  - `HIST.1.defect_attractor`
  - `file = <path>`
  - `line = 0`
  - message with the count of fix-like touches and the size of the window.

### Runtime shape

- A single history pass over `git log --numstat --pretty=...`, reusing `history_query.h` (#098).
- Vendor/generated/test paths are excluded by the same classifier from `file_classification.h`.
- Message classification is simple: regex matching, no NLP.
- Mechanical commits (>30 files + fix-like) are filtered automatically.

### Scope discipline

- Report-only by default, advisory, not a gate.
- No blame, SZZ, ownership, bus-factor.

## Done

- ✅ **Detector** (include/archcheck/scan/defect_attractor.h + src/scan/defect_attractor.cpp):
  - `isFixLikeCommit()` — regex matching over the subject line
  - `shouldSkipCommit()` — filtering out merge/empty/mechanical commits (>30 files)
  - `aggregateFixTouches()` — aggregation over production files (vendor/test excluded)
  - `calculateTopDecileThreshold()` — statistics: if <10 files then max, otherwise 90th percentile
  - `detect()` — return violations in descending order of fix_touches
  - **Code quality**:
    - ✅ NLOC ≤30 (refactored: `filterCandidates()` helper)
    - ✅ CCN ≤15 (max 12)
    - ✅ Arguments ≤5
    - ✅ clang-format
    - ✅ cppcheck (no warnings)

- ✅ **Wiring into CLI** (src/main.cpp::run_history):
  - Added includes for defect_attractor.h
  - run_history() now prints the SIZE.1 block first, then the HIST.1 block
  - HIST.1 stays advisory (exit 0 always)

- ✅ **Tests** (tests/unit/scan/defect_attractor_test.cpp):
  - 11 unit tests cover:
    - fix-like pattern matching (word boundaries: "fix" matches, "prefix/suffix/fixture" do not match)
    - 6 fixes in 1 file (12 files total) → 1 finding (top decile)
    - 4 fixes < floor (5) → no finding
    - merge commits skipped
    - mechanical fix (>30 files) skipped
    - vendor paths excluded
    - test files excluded
    - results sorted in descending order of fix_touches
    - different fix-like patterns recognized
    - empty history → no findings
    - <10 files → top-1 threshold

- ✅ **CMakeLists.txt**:
  - src/scan/defect_attractor.cpp added to the archcheck_core library
  - tests/unit/scan/defect_attractor_test.cpp added to archcheck_tests

- ✅ **Gates**:
  - clang-format: ✅ exit 0
  - cppcheck: ✅ exit 0
  - lizard: ✅ exit 0 (NLOC ≤30, CCN ≤15, args ≤5)
  - Debug build: ✅ exit 0
  - tests: ✅ 413/413 passed (11 defect_attractor tests + all the rest)
  - dogfooding: ✅ src/, include/, tests/ — no violations

## In progress

- (empty)

## Next steps

- (none, task closed)

## Limitations (documented)

- Spike over the last 200 commits (the window may be smaller on young repos).
- No temporal analysis (a spike over 14 days is not implemented in v1, but the structure will allow adding it).
- No blame/ownership graph (by design: cheap signal only).

## Key decisions

| Decision | Reason |
|---------|---------|
| History pass shared with #098 | Same technique, `history_query.h` is reused |
| Simple regex classification | Cheap signal, explainable, no NLP |
| Report/advisory only | The signal is probabilistic, not gate-grade |
| `line = 0` | History aggregates at the file level, no exact line |
| Top-decile if <10 files → max | Models the same as the 90th percentile at full population |
| >30 files in a fix commit → skip | Filtering out mechanical ones (lint, format, rename) by heuristic |

## Materials

- **Implementation**: 2 files (h+cpp), 206 lines of code
- **Tests**: 11 unit tests
- **Quality**: CCN≤15, NLOC≤30, all gates green
- **Reuse**: history_query, file_classification, violation structures

## Summary
**Status:** completed — HIST.1: top-decile of files by fix-like touches
(regex classification of commits, >30 files = skip mechanical ones) (commit cb6e09d:
`src/scan/defect_attractor.cpp`, 11 unit tests over synthetic history).
**Completed:** 2026-06-11
