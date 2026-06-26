# [CHEAP-DRIFT][SCAN] Indentation-Complexity Drift

**Date created:** 2026-06-10
**Date started:** —
**Status:** new
**Module:** SCAN / DIFF / REPORT
**Priority:** minor
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #029 (metric_regression_detection), #030 (baseline_file_flag), #093 (flag_argument), #094 (param_count_and_accretion), #101 (local_complexity_drift)

## Goal

Provide a rough text-only proxy for "the code got more deeply nested / branchier", without dragging in AST or a full complexity analyzer.

This is precisely a drift heuristic. It does not replace `lizard`, does not argue with cyclomatic complexity, and must not be treated as a precise complexity metric.

## Context

- Source of the assignment: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 7.
- The repo already has lizard in dogfood/static-analysis. So the new heuristic must answer a different question: "did it get noticeably worse relative to the base ref", not "what is a function's objective CCN".
- The current baseline mechanism stores violations, not per-file text metrics. So the realistic first pass is a comparison of current vs git ref / diff, not a new global snapshot format within this task.
- The signal is probabilistic and sensitive to formatter/style changes. So a default gate is forbidden.
- After #101 appeared, this task cannot be implemented as a separate parallel product: either it stays a narrow fallback/proxy inside `local_complexity_drift`, or it is closed as absorbed by the stronger function-aware signal.

## Execution plan

### Detection contract

- At the file or function-like region level, compute:
  - max indentation;
  - average indentation;
  - variance;
  - number of lines with deep indentation.
- `CMP.1.indentation_complexity_drift` arises when current is noticeably worse than the base ref on one or more thresholds.
- **Status relative to #101 (after design research 2026-06-11):** indentation is a proxy
  for classic metrics with an inherited correlation with volume (Hindle ICPC 2008;
  Gil & Lalouche EMSE 2017) and a format dependency (alignment, tabs — reproduced in
  `docs/research/local_complexity_drift_scorer_review.md`). Therefore: it does NOT enter the #101 score;
  this task is implemented only as a **file-level fallback** for
  files where #101's function-discovery did not fire (macro-heavy), and as
  diagnostic report fields. If after #101 the fallback turns out to be unneeded —
  close as absorbed.

### Runtime shape

- Work on non-empty, non-comment-only lines.
- Convert tabs to columns via a fixed `tab_width`.
- The first pass can be done at file level; finer-grained function ranges are allowed only if they come cheaply out of existing scanner logic.
- Run the comparison via git ref / diff; a separate saved metric baseline is not required.

### Noise control

- Document the sensitivity to mass reformat.
- It would be nice to be able to quickly disable files where the entire difference is caused by a formatter-only commit.
- Do not count empty lines and pure comment lines.

### Concrete plan in the current code

1. Do not write yet another comment-stripper:
   use the shared `text_scan.cpp` already needed by #093/#095.
2. Add `include/archcheck/scan/indentation_metrics.h` + `src/scan/indentation_metrics.cpp` with a pure file-level function:
   input — cleaned text;
   output — `IndentationMetrics { maxIndent, avgIndent, variance, deepLineCount }`.
3. A full snapshot can be computed via `collectNonVendoredSources()`, but the practical priority is comparing current vs base ref on changed files.
4. The diff path needs no hunk parser:
   `git::changedCppFiles()` from [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp) is enough,
   then read old/new contents via `GitObjectFileSource` and `DiskFileSource`.
5. Do not extend baseline JSON v1:
   the current baseline contract stores violations, not text metrics. This task must live on ref comparison, not on a new snapshot format.
6. The detector may return a file-level `Violation` with `line = 0`; there is no need to artificially pick the "deepest line".
7. Tests:
   new `tests/unit/scan/indentation_metrics_test.cpp`;
   1-2 compare scenarios in [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp), to verify it works via real refs.

## Fixtures & Tests

- `fixtures/indentation_complexity_drift/pass/shallow_baseline.cpp`
- `fixtures/indentation_complexity_drift/fail/deeper_current.cpp`
- `fixtures/indentation_complexity_drift/pass/comment_only_noise.cpp`
- `fixtures/indentation_complexity_drift/pass/tab_indentation.cpp`

Mandatory checks:

- growth of deep-indented lines is detected;
- comment-only noise has no effect;
- tabs count predictably;
- the result stays advisory/report only;
- the formatter-only false-positive profile is described explicitly.

## Readiness criteria

- No AST / libclang.
- Comparing current vs base ref is enough for v1.
- The metric does not claim CCN precision.
- By default it does not participate in gate semantics.

## Do not do

- Replace the rule with the existing `lizard`.
- Build a full parser of block structure.
- Treat this as a precise architectural metric.
- Enable a default gate.

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Fix the set of text metrics and thresholds for v1.
2. Implement the line classifier and indent counter.
3. Wire up the comparison against base ref.
4. Check the reaction to a real-world reformat diff.

## Key decisions

| Decision | Reason |
|---------|---------|
| Drift relative to base ref, not absolute complexity | Otherwise the check duplicates a linter/metric tool |
| File-level v1 is enough | It is the cheapest and most explainable start |
| Advisory/report only | High sensitivity to formatting noise |
| Separate from graph metrics | The signal is textual, not graph-derived |

## Planned files

| Area | Change |
|---------|-----------|
| `src/scan/` or `src/cheap_drift/` | Indentation metrics collector |
| `src/main.cpp` / diff comparison | Current vs base ref orchestration |
| `tests/unit/` | Metrics and noise suppression |
| `tests/integration/` | Ref-comparison scenarios |
| `fixtures/indentation_complexity_drift/` | `pass/` and `fail/` fixtures |

## Outcome (closure)

**Status:** dropped (absorbed)
**Date:** 2026-06-12

Closed per the verdict of #109: corpus validation (945 finding-commits across 84
repos, fixes A–K) proved that function-aware discovery (#101) reliably
covers real repos — a text-only indentation proxy as a fallback is not needed.
The scenarios it was conceived for (broken parse, macro-heavy code)
are closed pointwise: brace-guard, largest-#if-branch, scope-tracking.
