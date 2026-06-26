# [CHEAP-DRIFT][HISTORY] God-File Growth

**Created:** 2026-06-10
**Started:** 2026-06-11
**Status:** wip
**Module:** GIT / HISTORY / REPORT
**Priority:** minor
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #048 (drift_clean_checkout_methodology), #075 (trusted_diff_workflow), #097 (test_co_evolution)

## Goal

Find files that, by commit history, steadily inflate and become candidates for a future god file.

This is report-only analytics. Not to be confused with the already-shipped `Lakos.GodHeader`, which measures the fan-in of headers in the include graph.

## Context

- Source of the task: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 6.
- The name resembles `GodHeader`, but the meaning is different: here it's not about a graph hub, but about an accumulator file by size and change history.
- The signal needs `git log --numstat` plus current LOC. So the task lies closer to the history/report layer than to a regular `IRule`.
- By product positioning this is probabilistic analytics. A default gate is contraindicated here.

## Execution plan

### Detection contract

- A file is suspicious if all conditions hold simultaneously:
  - it is already big enough;
  - it grew noticeably over the history window;
  - it grew across several consecutive touching commits;
  - no meaningful shrink was observed.
- Concrete v1 defaults (algorithm over `git log --numstat`, window = the last
  `N=200` commits or 12 months, whichever is smaller):
  - "already big": current LOC ≥ P75 over the repo's production files (an adaptive threshold,
    not an absolute — like Arcan's thresholds from frequency analysis of the project);
  - "grew noticeably": net growth over the window ≥ +30% OR ≥ +300 lines;
  - "grew consecutively": ≥ 5 most recent touching commits with `added > deleted`;
  - "no shrink": not a single commit with `deleted - added >= 50` in the window;
  - all four conditions — AND; rare by construction, otherwise raise the thresholds.
- Final finding:
  - `SIZE.1.god_file_growth`
  - `file = <path>`
  - `line = 0`
  - message with current LOC, net growth, and number of growth touches.

### Runtime shape

- Do one repository-wide pass over `git log --numstat`, not a separate call per file.
- Take current LOC from the current tree.
- Exclude vendor/generated/test code via the existing classification where applicable.
- If history is too expensive on large repos, the task must fix window limits and graceful degradation.

### Scope discipline

- First pass — only report/advisory analytics.
- No line-level blame.
- No ownership / bus-factor / SZZ.
- No attempt to derive the "true architectural reason" for a file's growth.

### Concrete plan in the current code

1. Don't read history via N `git log` calls per file:
   a single shared parser `include/archcheck/git/history_query.h` + `src/git/history_query.cpp` over the extracted `git_exec.cpp` is needed.
2. Fix the query format right away:
   `git log --numstat --format=%H%x1f%s` with a limit on the number of commits.
   This is enough for both #098 and #100.
3. Compute current LOC once over the current tree:
   `DiskFileSource` + `collectNonVendoredSources()` already give a list cleaned of vendor/test, from which a `path -> currentLoc` can be built.
4. Keep the signal itself as a separate file-level detector that aggregates history by path and outputs `Violation { ruleId, file, line=0, message }`.
5. Don't try to cram this into `RegressionReport`:
   history is unrelated to the current structural diff model, and `RegressionReport` is currently tailored to graph-vs-graph.
6. In `src/main.cpp` it's better to hang this as a separate report path next to the existing diff/duplication helpers.
   If the task is implemented before the general history CLI, a temporary text-only subpath is acceptable, but not via `IRule`.
7. Integration git scenarios can first be kept in [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp), because it already has `TempDir`, `initRepo`, `commitAll`.
   Extract a shared test helper only if the history suite really grows.

## Fixtures & Tests

- `fixtures/god_file_growth/fail/steady_growth.log`
- `fixtures/god_file_growth/pass/growth_then_shrink.log`
- `fixtures/god_file_growth/pass/vendor_ignored.log`
- `fixtures/god_file_growth/pass/small_file.log`

Required checks:

- steady growth over history produces a finding;
- growth followed by a meaningful shrink does not produce a false positive;
- vendor/generated/test files are excluded;
- message shows the window/growth summary;
- the rule does not affect the exit code by default.

## Acceptance criteria

- Report-only or advisory-only, but not a gate.
- Clearly disambiguated in terminology from `Lakos.GodHeader`.
- No N `git log` shell calls per file.
- There are synthetic fixtures for the history parser.

## Don't do

- SZZ.
- Blame/ownership map.
- AI scoring "how dangerous the file is".
- An attempt to hang this signal on a default CI fail.

## Done

- `include/archcheck/git/history_query.h` + `src/git/history_query.cpp`: queryCommitHistory + parseHistoryOutput (single git log pass)
- `include/archcheck/scan/god_file_growth.h` + `src/scan/god_file_growth.cpp`: GodFileGrowthDetector with 4 conditions
- CLI: `archcheck --history <path>` (advisory-only, always returns 0)
- Tests: 5 unit tests for parseHistoryOutput + 1 end-to-end GodFileGrowthDetector + an integration test
- All 402 tests green, archcheck self-check clean, formatting OK
- Found a real god-file: src/config/config_loader.cpp (413 LOC, +160 net, 5 growth commits)

## Next steps

1. Design the history parser on a single `git log --numstat` pass.
2. Fix the thresholds and history window for the first pass.
3. Add fixtures steady-growth vs grow-then-shrink.
4. Check the cost on an average OSS repo.

## Key decisions

| Decision | Reason |
|---------|---------|
| Repository-wide history pass | Otherwise the rule becomes too expensive |
| Disambiguate from `Lakos.GodHeader` both by id and in docs | These are two different classes of signal |
| Report/advisory only | The metric is probabilistic, not gate-grade |
| `line = 0` | This is a file-level history signal with no precise line |

## Planned files

| Area | Change |
|---------|-----------|
| `src/git/` or history analytics | Parser `git log --numstat` |
| `src/main.cpp` / optional report path | Wiring of the report-only signal |
| `tests/unit/` | History aggregation |
| `tests/integration/` | Cost and end-to-end scenarios |
| `fixtures/god_file_growth/` | Synthetic history fixtures |

## Outcome
**Status:** completed — HIST signal: 5+ consecutive growth commits of a file,
CLI `--history` (advisory) (commit cb6e09d: `src/scan/god_file_growth.cpp` +
unit tests on synthetic history — file fixtures not needed, the signal is history-based).
**Completed:** 2026-06-11
