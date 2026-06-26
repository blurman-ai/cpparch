# [GRAPH][DIFF] Metric regression detection in RegressionReport

**Created:** 2026-05-28
**Started:** 2026-05-28
**Status:** completed
**Module:** GRAPH / DIFF
**Priority:** major
**Difficulty:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #009 (ai_drift_regression_rules), #027 (coverage_90_percent)

## Goal

Extend `RegressionReport` with metric regressions (growth of max chain length, the appearance of new god-headers,
CCD/ACD/NCCD deltas) and add tests that detect these regressions via a git comparison of two revisions.

## Context

Right now `RegressionReport` can only compare edges and SCC cycles (`addedEdges`, `removedEdges`,
`grownCycles`). The MVP declares three additional violation categories:

- **Lakos chain length** — default threshold 10; a regression when it is exceeded or grows between revisions.
- **God-headers (fan-in)** — default threshold 30; nodes with fan-in above the threshold that were not in the baseline —
  a regression.
- **CCD/ACD/NCCD** — aggregate Lakos metrics; growth of ACD or NCCD between revisions is a regression.

`computeIncludeDepths` (chain depth) already exists in `algorithms.h`; fan-in and CCD/ACD/NCCD need to be added.

Integration tests through real git repositories already work (see `tests/integration/diff/git_diff_test.cpp`);
new scenarios are added there too.

### Strategy for test git repositories

Studying the patterns from libgit2, gitoxide, go-git revealed three approaches:

1. **Programmatic creation (our approach)** — `mkdtemp` + `git init/commit` via `std::system`. Already used
   in `git_diff_test.cpp`. Sufficient for all #029 tests: each scenario is 2 commits (before/after),
   created in a `TempDir` and removed after the test. Key precautions: fix `user.email/name`,
   disable `commit.gpgsign`.

2. **Pre-built bare repo in `tests/resources/`** (libgit2 style) — a binary `.git` directory is committed
   to the repo. Fast, but opaque and hard to change. **Not used.**

3. **Fixture script + `.bundle` file** (gitoxide style) — a bash script creates a complex history,
   the result is packed with `git bundle create`. The script and bundle are committed; the test clones the bundle.
   **Fallback option:** apply it if we later need to scan the N most recent commits
   (the "scan git log" feature), where programmatically creating 50+ commits would become too slow.
   Place it under `tests/fixtures/bundles/`.

**Conclusion for #029:** the programmatic approach, the `TempDir` + `initRepo` + `commitAll` pattern, is already ready.

## Execution plan

### Algorithms (`graph/algorithms`)

- [ ] Add `computeFanIn(const DependencyGraph&) → vector<size_t>` (in-degree of each node).
- [ ] Add `computeCCD(const DependencyGraph&) → size_t`
      (∑ |reachableFrom(n)| + 1 over all n; the +1 counts the node itself).
- [ ] Add `computeGraphMetrics(const DependencyGraph&) → GraphMetrics` — aggregates
      maxChainLength, maxFanIn, CCD, ACD (`double = CCD / N`), NCCD (`double = ACD / log2(N+1)`).

### Structures (`diff/regression_report.h`)

- [ ] Add `struct MetricDelta { size_t baseline; size_t current; }` (for integer metrics).
- [ ] Extend `RegressionReport`:
  ```cpp
  std::optional<MetricDelta>  chainLengthGrown;   // set only when current.max > baseline.max
  std::vector<std::string>    newGodHeaders;       // nodes crossing fan-in threshold (default 30)
  std::optional<double>       nccdDelta;           // set when NCCD grew
  ```
- [ ] Update `hasRegression()`: add the checks `chainLengthGrown`, `!newGodHeaders.empty()`,
      `nccdDelta > 0`.

### Implementation (`diff/regression_report.cpp`)

- [ ] Accept `MetricThresholds { size_t chainLengthLimit = 10; size_t godHeaderFanIn = 30; }` as a second
      argument to `buildRegressionReport` (with default values — zero-config stays).
- [ ] Compute `GraphMetrics` for baseline and current; fill the new fields.

### Text report (`report/writeTextReport`)

- [ ] Add lines to the output:
  ```
  chain_length:   <N>  [+<delta>]     (only if it grew)
  god_headers:    <K>  new: h1, h2    (only if present)
  nccd:           <F>  [+<delta>]     (only if it grew)
  ```

### Unit tests (`tests/unit/diff/regression_report_test.cpp`)

- [ ] `chain length grew → chainLengthGrown set, hasRegression() true`
- [ ] `chain length did not grow → chainLengthGrown nullopt`
- [ ] `new god-header added → newGodHeaders non-empty, hasRegression() true`
- [ ] `NCCD grew → nccdDelta > 0, hasRegression() true`
- [ ] `writeTextReport emits chain_length / god_headers / nccd lines only when grown`

### Integration tests (`tests/integration/diff/git_diff_test.cpp`)

- [ ] `git: PR adds deep include chain → chainLengthGrown detected`
  Scenario: baseline = `a→b→c` (depth 2); the PR adds `b→d→e→f→...` (depth > baseline max).
- [ ] `git: PR creates god-header → newGodHeaders non-empty`
  Scenario: baseline = a graph without high fan-in; the PR adds 31+ incoming edges to a single node.
- [ ] `git: PR increases NCCD → nccdDelta detected`
  Scenario: baseline = a sparse graph; the PR densifies dependencies, NCCD grows.
- [ ] `git (memory variant): chain length regression detected via GitObjectFileSource`
  Repeats the first scenario through the in-memory path (without a worktree checkout).

### Fixtures (optional, if separate fixture projects are needed)

- [ ] `fixtures/chain_length/pass/` — a chain within the threshold
- [ ] `fixtures/chain_length/fail_deep/` — a chain exceeding the threshold
- [ ] `fixtures/god_header/pass/`
- [ ] `fixtures/god_header/fail_fanin/`

## Done

- **2026-05-28** — the entire plan was implemented in commit `c480e39` (the same commit that created this file; the "Done" section was not filled in at the time — the task file was mistakenly left in `new/` with status `new` until the backlog review on 2026-06-11):
  - `computeFanIn` O(E) + `GraphMetrics` (maxChainLength / maxFanIn / CCD / ACD / NCCD) + `computeGraphMetrics` in `graph/algorithms`;
  - `MetricDelta`, `MetricThresholds` (chainLengthLimit=10, godHeaderFanIn=30), the fields `chainLengthGrown` / `newGodHeaders` / `nccdDelta` in `RegressionReport`, `hasRegression()` extended;
  - `writeTextReport` emits chain_length / god_headers / nccd only on a regression;
  - unit tests (+160 lines of `regression_report_test.cpp`) and 4 git integration tests (+93 lines of `git_diff_test.cpp`), the coverage gate passed (lines 95.9%).

## Next steps

- Closed. The optional fixture projects (`fixtures/chain_length/`, `fixtures/god_header/`) were not created — the scenarios are covered by programmatic git tests, separate fixtures were not needed.

## Key decisions

| Decision | Reason |
|---------|---------|
| `MetricThresholds` as an argument with default values | Zero-config stays, but the thresholds can be overridden from the YAML config later |
| A separate `computeGraphMetrics` instead of direct calls | Avoids computing depths and fan-in twice in `buildRegressionReport` |
| `newGodHeaders` — a list of names, not NodeId | The names are needed for the text report; NodeId is an implementation detail |
| `nccdDelta` — `optional<double>`, not bool | Allows showing the numeric change in the report |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/graph/algorithms.h` | + `computeFanIn`, `GraphMetrics`, `computeGraphMetrics` |
| `src/graph/algorithms.cpp` | Implementation of the new functions |
| `include/archcheck/diff/regression_report.h` | + `MetricDelta`, `MetricThresholds`, new fields in `RegressionReport` |
| `src/diff/regression_report.cpp` | Filling the new fields |
| `src/report/text_reporter.cpp` | New output lines |
| `tests/unit/diff/regression_report_test.cpp` | +5 unit tests |
| `tests/integration/diff/git_diff_test.cpp` | +4 git integration tests |

## How it works

`buildRegressionReport` computes `computeGraphMetrics` for the baseline and current graphs and fills three
optional fields: `chainLengthGrown` (growth of max include depth), `newGodHeaders` (nodes that crossed the
fan-in threshold of 30 for the first time), `nccdDelta` (growth of Normalized ACD — catches densification of
the graph without any single large edge). The thresholds are `MetricThresholds` with default values, zero-config preserved.

## Outcome

**Status:** completed
**Completed:** 2026-05-28 (actual, commit `c480e39`); the file was closed on 2026-06-11 following the backlog review.
**Reason for the late closure:** the implementation and the task file landed in one commit, the checkboxes and status were not updated — for 2 weeks the task looked unstarted. No duplicated work occurred, but #057 and TASK_TRACKER referenced it as not done.
