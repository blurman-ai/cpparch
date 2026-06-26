# [CHEAP-DRIFT][DIFF] SATD Delta

**Created:** 2026-06-10
**Started:** 2026-06-11
**Status:** wip
**Module:** GIT / DIFF / REPORT
**Priority:** major
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #024 (in_memory_fs_for_diff), #075 (trusted_diff_workflow), #093 (flag_argument)

## Goal

Warn that a PR adds new self-admitted technical debt markers:

- `TODO`
- `FIXME`
- `HACK`
- `XXX`
- `TEMP`
- `temporary`
- `workaround`
- `quick fix`
- `dirty`

This is a review signal, not an architectural violation.

## Context

- Source of the task: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 4.
- The rule must live **only** in a delta context. A full scan over the legacy tree is nearly useless here and conflicts with the "gate only on new drift" principle.
- The current `archcheck --diff` already does structural comparison between git refs. This task should be fitted alongside the existing diff pipeline, not built up as a separate half-tool.
- In the product framing this is not a default architectural rule. So: advisory-first, opt-in gate at most for the narrow case `FIXME/HACK without issue id`.

## Execution plan

### Detection contract

- `SATD.1.new_satd_marker`:
  - any new marker on an added line;
  - advisory by default.
- `SATD.2.untracked_fixme_or_hack`:
  - `FIXME` or `HACK` on an added line without an issue id;
  - remains a gate-candidate, but is not enabled in the default gate without explicit configuration in the future.

### Algorithm (trivial, pinned down so it doesn't drift)

1. `collectAddedLines()` (see "Concrete plan", item 2) → `{file, line, text}`.
2. The marker is searched for **only in the comment part** of the added line: after `//` or
   inside `/* */` (rough detection: position of `//` outside string literals; lines
   without a comment are skipped). Otherwise `dirty` inside a code identifier would give an FP.
3. `SATD.1` regex (case-insensitive, on word boundaries):
   `\b(TODO|FIXME|HACK|XXX|TEMP)\b|\btemporary\b|\bworkaround\b|\bquick.?fix\b|\bdirty\b`.
4. `SATD.2`: marker `FIXME|HACK` and NO issue-id on the same line:
   `([A-Z][A-Z0-9]+-\d+)|(#\d+)|(\b(gh|issue)[-/]\d+)` (JIRA / GitHub / generic).
5. Case handling: `TODO/FIXME/HACK/XXX/TEMP` — UPPERCASE only (lowercase `todo`
   in comment prose is not a marker); the word forms (`workaround`, `temporary`,
   `dirty`, `quick fix`) — case-insensitive.
6. One finding per line (not per marker); `Violation{ruleId, file, line, message=line text truncated to 120}`.

### Runtime shape

- Operate only on added lines from the unified diff.
- Full-tree scan out of scope.
- The issue-pattern for the first pass can be hardcoded if a separate config surface requires schema work.
- If line mapping already gives an exact path and line number, the finding must live in the current `Violation` contract.

### Noise control

- Do not count removed or context lines.
- Ignore markers inside removed code blocks.
- Case sensitivity must be pinned down by tests, not left "however it turns out".

### Concrete plan in the current code

1. The current code has no reusable git command helper:
   `runGit()` lives in an anon namespace in [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp).
   The first step of this task is to extract it into `include/archcheck/git/git_exec.h` + `src/git/git_exec.cpp`, otherwise #096/#097/#098/#100 will duplicate process-launch code.
2. Add `include/archcheck/git/diff_query.h` + `src/git/diff_query.cpp` with two public primitives:
   `collectAddedLines(...)` for unified diff `--unified=0`;
   `collectNumstat(...)` for `--numstat` (the same API is immediately reused by #097).
3. `collectAddedLines(...)` must support both current diff modes:
   `a..b` and `a..WORKTREE`, with the same WORKTREE semantics already pinned down in `changedCppFiles()`.
4. Keep the SATD detector itself as a separate small file (`src/scan/satd_scan.cpp` or an adjacent flat helper) that takes `AddedLine` records and returns a `rules::ViolationList`.
5. Do the integration alongside [src/main.cpp](~/projects/cpparch/src/main.cpp)::`run_diff()`:
   do not extend `RegressionReport`,
   do not change `archcheck::diff::writeTextReport(...)`,
   but print the SATD block after the structural diff summary.
6. If this same task needs machine-readable diff output, you will first have to refactor `dispatch_format()` / `dispatch_diff()` in [src/main.cpp](~/projects/cpparch/src/main.cpp):
   right now `--format` works only for `run_check()`, and `--diff` JSON is not supported at all.
7. Tie the tests to the current structure:
   parser level — new `tests/unit/git/diff_query_test.cpp`;
   marker logic — new `tests/unit/scan/satd_scan_test.cpp`;
   repo-level diff — extension of [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/satd_delta/pass/hack_with_issue.diff`
- `fixtures/satd_delta/fail/todo_added.diff`
- `fixtures/satd_delta/fail/fixme_without_issue.diff`
- `fixtures/satd_delta/pass/context_only.diff`

Mandatory checks:

- the marker is caught only on added lines;
- `HACK ABC-123: ...` does not trigger `SATD.2`;
- `FIXME: ...` without an issue id triggers `SATD.2`;
- text/json output stays compatible with the current contract;
- the rule does not scan the whole legacy tree.

## Acceptance criteria

- Diff-only.
- Advisory-first.
- No schema change.
- No new separate CLI tool outside `archcheck`.

## Don't do

- Full repository grep for SATD.
- Ownership / blame analysis.
- Automatic recognition of "correct" issue tracker formats beyond simple regex.
- Gate by default for any `TODO`.

## Done

1. Extract `runGit()` from the anon namespace of git_state.cpp into a shared helper:
   - `include/archcheck/git/git_exec.h` + `src/git/git_exec.cpp` (82 lines)
   - Updated git_state.cpp to use the shared helper
2. Create `include/archcheck/git/diff_query.h` + `src/git/diff_query.cpp`:
   - `collectAddedLines()` — parsing unified diff --unified=0
   - `collectNumstat()` — parsing git diff --numstat
   - Support for both modes: a..b and a..WORKTREE
3. Create the SATD detector:
   - `include/archcheck/scan/satd_scan.h` + `src/scan/satd_scan.cpp` (156 lines)
   - `detectSatdMarkers()` implements SATD.1 (any marker) and SATD.2 (FIXME/HACK without issue-id)
   - Support for all regular markers from the spec
   - The marker is caught only in comments (// or /* */)
   - One violation per line, truncation to 120 characters
4. Integration into src/main.cpp:
   - Added collection of added lines via `collectAddedLines()`
   - Added a call to `detectSatdMarkers()` in `runDiffFullPath()`
   - The SATD block is printed after the structural report (advisory-only, does not gate)
5. Fixtures in fixtures/satd_delta/:
   - pass/: hack_with_issue.diff, context_only.diff, dirty_in_code.diff
   - fail/: todo_added.diff, fixme_without_issue.diff, hack_without_issue.diff, temporary_marker.diff, workaround_marker.diff
6. Tests:
   - tests/unit/git/diff_query_test.cpp (2 basic tests, 21 lines)
   - tests/unit/scan/satd_scan_test.cpp (22 tests, 184 lines)
   - tests/integration/diff/git_diff_test.cpp (1 integration test added)
   - All 381 tests pass

## In progress

- (none)

## Next steps

- Ready for review and merge

## Key decisions

| Decision | Reason |
|---------|---------|
| Added lines only | This is new debt, not a rewrite of all the legacy |
| `SATD.2` narrower than `SATD.1` | Not every `TODO` should even theoretically break CI |
| Hardcoded defaults acceptable in v1 | The current schema contract is not ready for cheap-drift knobs |
| Alongside the existing diff pipeline | Don't proliferate a second way to understand git diff |

## Planned files

| Area | Change |
|---------|-----------|
| `src/git/` or diff orchestration | Access to added lines |
| `src/main.cpp` / cheap-drift pass | Wiring up the SATD detector |
| `tests/unit/` | Marker / regex logic |
| `tests/integration/` | Diff fixtures |
| `fixtures/satd_delta/` | `pass/` and `fail/` scenarios |

## Summary
**Status:** completed — SATD markers (TODO/FIXME/HACK/XXX) on added diff lines,
advisory in `--diff` (commit cb6e09d: `src/scan/satd_scan.cpp` + unit tests +
`fixtures/satd_delta/`). Mechanics details are in the task body and CHANGELOG (f80a83c).
**Completion date:** 2026-06-11
