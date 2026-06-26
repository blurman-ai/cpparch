# [CLI][REPORT] --baseline <file> flag for check mode

**Created:** 2026-05-28
**Started:** 2026-05-28
**Completed:** 2026-05-28
**Status:** completed
**Module:** CLI / REPORT
**Priority:** major
**Difficulty:** S (a few hours)
**Blocks:** —
**Blocked by:** —
**Related:** #016 (graph_baseline_contract — different meaning, graph), #029 (metric_regression_detection — metrics in diff mode)

## Goal

Add `--baseline <file>` and `--save-baseline <file>` to check mode, so that legacy projects
can adopt archcheck in CI without rewriting: the first run records the violations, subsequent runs
report only new ones.

## Context

Right now `archcheck [path]` emits all violations at once. On a legacy project (LLVM, Qt, etc.)
this can be 200+ violations — overwhelming, the team doesn't know where to start and sets
the tool aside. This is the classic adoption barrier for architectural checkers.

References:
- **ArchUnit** `FreezingArchRule` — saves known violations to a file, CI fails only on new ones.
- **clang-tidy** `--config` with `Checks: -<rule>` — a similar idea, different UX.
- **ESLint** `--no-error-on-unmatched-pattern` + `.eslintignore`.

`--diff main..HEAD` partly solves the task (shows only new edges), but requires
git context and doesn't work for `archcheck [path]` without a git repository.

Example workflow for a legacy project:
```bash
# Once: record the current state
archcheck --save-baseline .archcheck.baseline /path/to/project

# In CI (and locally): only new violations
archcheck --baseline .archcheck.baseline /path/to/project
# exit 0 while the team doesn't add new violations
# exit 1 if something new appeared relative to the baseline
```

## Baseline file format

Simple JSON, schema v1:
```json
{
  "version": 1,
  "violations": [
    {"rule": "SF.9", "file": "src/foo.h", "line": 0, "message": "cycle: a → b → a"},
    {"rule": "SF.7", "file": "include/bar.h", "line": 12, "message": "using namespace std"}
  ]
}
```

The violation identifier for matching: `(rule, file, line)`. `message` is informational.
Two violations are considered equal if all three fields match.

## Execution plan

### Data structure

- [ ] Add `ViolationBaseline` to `include/archcheck/report/violation_baseline.h`:
  ```cpp
  struct ViolationBaseline {
     std::vector<Violation> known;
  };
  ViolationBaseline loadBaseline(const std::filesystem::path&);  // throws on parse error
  void saveBaseline(const ViolationBaseline&, const std::filesystem::path&);
  ViolationList filterNew(const ViolationList& all, const ViolationBaseline& baseline);
  ```

### CLI

- [ ] Add to `print_help()` the lines:
  ```
  archcheck --save-baseline <file> [path]   (save current violations as baseline)
  archcheck --baseline <file> [path]        (report only new violations vs baseline)
  ```
- [ ] Add handling of `--save-baseline` and `--baseline` to `dispatch()`.
- [ ] Have `run_check` accept `std::optional<std::filesystem::path> baselinePath`:
  - if `--save-baseline`: run check, save all violations to the file, exit 0.
  - if `--baseline`: load the baseline, run check, filter, report only new ones.
  - Baseline load error → exit 2 + message to stderr.

### Report

- [ ] In the text report under `--baseline`, add a summary line:
  ```
  suppressed: 42 known violations (run without --baseline to see all)
  ```
  — if there are suppressed violations.

### Unit tests (`tests/unit/report/violation_baseline_test.cpp`)

- [ ] `saveBaseline + loadBaseline` — round-trip is idempotent
- [ ] `filterNew` — a violation from the baseline does not appear in the output
- [ ] `filterNew` — a violation with a different `line` is considered new
- [ ] `filterNew` — a violation with a different `rule` is considered new
- [ ] `loadBaseline` on a non-existent file → exception with a clear message
- [ ] `loadBaseline` on invalid JSON → exception

### Integration test (`tests/integration/cli/` or in `main_test.cpp`)

- [ ] `--save-baseline` creates a file, `--baseline` returns exit 0 on the same set
- [ ] `--baseline`: an added violation → exit 1, the new violation in the output
- [ ] `--save-baseline` on clean code → empty violations array in the JSON

## Done

- `include/archcheck/report/violation_baseline.h` — `ViolationBaseline`, `BaselineError`, `saveBaseline`, `loadBaseline`, `filterNew`
- `src/report/violation_baseline.cpp` — implementation: JSON save/load with manual escape/unescape, `filterNew` via `set<tuple<rule,file,line>>`
- `src/main.cpp` — `--save-baseline <file>` and `--baseline <file>` in the CLI; extracted `trySaveBaseline`, `tryLoadAndFilter`, `applyBaselineAndReport`, `dispatch_baseline`, `dispatch_format` (made dispatch cleaner along the way)
- `tests/unit/report/violation_baseline_test.cpp` — 9 tests: round-trip, special chars, filterNew variants, missing file, invalid content
- 191/191 tests green, lizard clean

## How it works

**File format.** The same JSON that `--format json` emits, but without `summary`:
```json
{ "version": 1, "violations": [
    {"rule": "SF.7", "file": "include/a.h", "line": 12, "message": "using namespace std"},
    {"rule": "SF.9", "file": "src/b.h", "line": 0, "message": "cycle: b → c → b"}
] }
```

**save_baseline.** Runs a full check, serializes all violations, always exit 0.

**load + filter.** Loads the file, builds a `set<(rule, file, line)>`, filters the current violations — only those not present in the set make it to the output. Suppressed violations are shown with a `suppressed: N known violation(s)` line.

**Matching key** — the triple `(ruleId, file, line)`. Message does not participate in the comparison — more stable across message refactors.

**Controlled by.** CLI flags `--save-baseline <file>` and `--baseline <file>`. The file format is JSON v1 (next to the `--format json` output, but without the summary section).

**Related to.** `archcheck::report` — next to `text_reporter` and `json_reporter`. `ViolationBaseline` knows nothing about the graph (`graph/baseline.cpp` is a different concept).

**Diagnostics.** On a load error (`file not found`, `not a valid archcheck baseline`) — exit 2 + message to stderr. A violation on a specific line of a file includes the line number in the message.

## Key decisions

| Decision | Reason |
|----------|--------|
| Matching by `(rule, file, line)`, not by `message` | Message can change when a rule is refactored; the triple is more stable |
| `--save-baseline` always exit 0 | The first run records the baseline, doesn't fail CI |
| Suppressed count in the report | The user sees that the baseline is active and how many violations are hidden |
| JSON, not a custom format | Easy to read/edit by hand; nlohmann or by hand (the project doesn't pull in nlohmann — do it by hand, the way baseline.cpp does YAML by hand) |
| A separate module `report/violation_baseline` | Don't mix with graph-baseline (#016); different concepts that share a word in the name |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/report/violation_baseline.h` | new |
| `src/report/violation_baseline.cpp` | new |
| `src/main.cpp` | + `--baseline`, `--save-baseline` in dispatch + print_help |
| `src/CMakeLists.txt` | + `violation_baseline.cpp` |
| `tests/unit/report/violation_baseline_test.cpp` | new |
| `tests/CMakeLists.txt` | + new test |
