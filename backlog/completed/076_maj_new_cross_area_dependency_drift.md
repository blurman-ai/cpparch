# [DIFF][GRAPH] Zero-config drift signal: new cross-area dependencies in `--diff`

**Date created:** 2026-06-02
**Date started:** 2026-06-02
**Date completed:** 2026-06-02
**Status:** done
**Module:** DIFF / GRAPH / REPORT
**Priority:** major
**Related:** #018 (git_diff_analysis), #009 (drift_regression_rules), #057 (lakos_fanout_coupling_checks), #075 (mvp_v1_trusted_diff_workflow)

## Goal

Add a zero-config signal to the main diff path:

> a new cross-area dependency channel `A -> B` appeared that was not in the baseline.

This is a level above the ordinary `addedEdges`: not "a new include between files", but
"a new link appeared between large areas of the project".

## Context

Before the task, `archcheck` already knew how to:

- new file-level edges;
- new/grown cycles;
- growth in chain length / NCCD / appearance of a god-header.

But the code had no signal for the question:

> "did a new link appear between two areas of the project that previously did not depend on each other?"

Meanwhile, the research `graph_probe.py` already had a close idea,
`[MODULE]` (`directory = module`) as a snapshot heuristic. So the signal is useful,
but lived outside the product diff/report core.

## How it works

In `RegressionReport`, a field was added:

- `newCrossAreaDependencies`

Each element stores:

- `fromArea`
- `toArea`
- `edgeCount` — how many file-level edges form this new channel in current
- `sampleFrom` / `sampleTo` — a concrete example for manual review

### Area heuristic (`area`)

This is a **zero-config area**, not a config-defined module:

- if the union of `baseline + current` has several top-level directories, the area = the first path segment (`src`, `tests`, `plugins`);
- if the whole project lives under one common top-level, the area = the first two segments (`src/core`, `src/net`) — otherwise everything would collapse into a single `src`.

The classifier is built **on the union of baseline and current paths**, so that a new
top-level directory does not change the granularity between snapshots and does not produce false
"new" pairs.

### Semantics

- if any channel `A -> B` already existed in the baseline, new file-level edges
  within that pair do not count as a new area signal;
- if any channel `A -> B` appears in current for the first time, one drift hit is reported
  per area pair;
- intra-area edges (`A -> A`) are ignored.

## Report output

The signal is included in the ordinary `buildRegressionReport()` and the text diff report.

New report fields:

- `new_area_deps: N`
- section `new_cross_area_dependencies:`

No additional CLI flags are required.

## Verified

Without running build/tests in this session, the following were added:

- unit tests in `tests/unit/diff/regression_report_test.cpp`
- integration test in `tests/integration/diff/git_diff_test.cpp`

Cases covered:

- the first `tests -> src` link is reported once per area pair;
- growth of an already-existing pair does not count as a "new" link;
- the text report prints the new section;
- git-based diff catches a new cross-area channel in a temp-repo scenario.

## Key decisions

| Decision | Reason |
|---------|---------|
| Do zero-config **areas**, not wait for runtime `modules` | a useful signal is needed already in the `v0.1` diff-core |
| Build the classifier on the union of `baseline + current` | a new top-level directory must not break the comparison |
| One hit per area pair, not N hits per file-level edge | less noise, the signal stays architectural |
| Keep `sampleFrom/sampleTo` | a finding must be openable and checkable by hand |
| Do not mix this with `layers/forbidden/independence` | the real policy layer remains a `v0.2` task |

## How it works

- [include/archcheck/diff/regression_report.h](../../include/archcheck/diff/regression_report.h)
  — `NewCrossAreaDependency` and the new field in `RegressionReport`
- [src/diff/regression_report.cpp](../../src/diff/regression_report.cpp)
  — area classifier, cross-area edge aggregation, detection of new pairs and text output
- [tests/unit/diff/regression_report_test.cpp](../../tests/unit/diff/regression_report_test.cpp)
  — unit coverage of the signal
- [tests/integration/diff/git_diff_test.cpp](../../tests/integration/diff/git_diff_test.cpp)
  — git-based integration scenario

## What controls it

- the ordinary `archcheck --diff <revspec> [path]`
- zero-config path heuristic, without `.archcheck.yml`

## What it relates to

- `graph::addedEdges()` / `graph::grownSccs()` — already-existing diff core
- `graph_probe.py` — the research predecessor of the idea
- the future config-policy layer (`layers` / `forbidden` / `independence`) — the precise successor of this heuristic signal

## Diagnostics

If the signal looks noisy:

1. check how exactly the heuristic sliced the areas by paths;
2. open `sampleFrom -> sampleTo` and make sure the include is real;
3. remember that `include/` + `src/` of the same library may look like a new area-pair link in zero-config mode;
4. for precise boundary enforcement, switch to config-defined modules in `v0.2`.
