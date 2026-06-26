# [CHEAP-DRIFT][DIFF] Test Co-Evolution Signal

**Date created:** 2026-06-10
**Date started:** 2026-06-11
**Status:** wip
**Module:** GIT / DIFF / REPORT
**Priority:** major
**Complexity:** small
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #024 (in_memory_fs_for_diff), #075 (trusted_diff_workflow), #096 (satd_delta)

## Goal

Highlight PRs where production code changes noticeably while tests do not move at all or barely move.

This is a review-prioritization signal, not an architectural rule and not a surrogate for test coverage.

## Context

- Source of the assignment: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 5.
- The task is especially useful in the CI review flow, where there is already a `git diff --numstat`-level signal and a cheap question is needed: "did the tests get touched at all?".
- The repository already has `scan/file_classification.h` with test/vendor/generated classification logic. This task should, where possible, lean on the existing taxonomy rather than introducing a second incompatible set of path rules.
- The signal is fundamentally advisory-only by default. It does not prove the absence of tests and must not break CI without an explicit opt-in.

## Execution plan

### Detection contract

- Collect production churn and test churn from `git diff --numstat`.
- Count separate buckets:
  - production paths;
  - test paths;
  - everything else.
- A finding arises when:
  - production churn is above a minimum threshold;
  - test churn is below a threshold or below a ratio relative to production churn.
- Concrete v1 defaults (calibrate on dogfood + 2-3 OSS PRs):
  - `prod_churn = added+deleted` over the production bucket; finding when
    `prod_churn >= 80` lines AND `test_churn == 0`;
  - soft variant: `prod_churn >= 200` AND `test_churn / prod_churn < 0.05`;
  - rename-only pairs (numstat with `=>`) excluded from churn of both buckets;
  - header-only changes (only `.h` without `.cpp`) — do not exclude (this is code too),
    but document the bucket composition in the message so the reviewer sees the reason;
  - one finding per diff: `TEST.1.prod_changed_tests_silent`,
    message = `prod +A/-D across N files, tests +0/-0` (real numbers).

### Runtime shape

- Base the first pass on numstat and the current file classification.
- Docs-only PRs must not fire.
- Rename-only and generated/vendor cases must either be ignored or explicitly documented as limitations.
- For a `Violation`, `file = "<diff>"` and `line = 0` are enough if there is no precise line anchor; inventing fake line numbers is not needed.

### Scope discipline

- Do not try to compute the "semantic significance" of a test change.
- Do not build coverage integration.
- Do not turn the rule into a hard gate by default.

### Concrete plan in the current code

1. Do not parse `git diff --numstat` a second time separately from #096:
   use the same `git/diff_query.cpp` and its `collectNumstat(...)`.
2. Build path classification from already-existing functions, not from a new glob list:
   `hasProjectExtension()` from [src/scan/project_files.cpp](~/projects/cpparch/src/scan/project_files.cpp),
   `pathHasTestDir()`, `isTestBasename()`, `pathHasVendoredDir()`, `isVendoredFile()` from [include/archcheck/scan/file_classification.h](~/projects/cpparch/include/archcheck/scan/file_classification.h).
3. Define the production bucket as "project extension && not test && not vendor".
   This is enough for v1 and it matches already-shipped exclusions.
4. Keep the detector flat and small (`src/scan/test_co_evolution.cpp` or a neighboring helper), input:
   `std::vector<NumstatEntry>`, output: either empty, or a single `Violation` with `file = "<diff>"`, `line = 0`.
5. Integrate it next to `run_diff()` in [src/main.cpp](~/projects/cpparch/src/main.cpp), not via `IRule` and not via `RegressionReport`.
6. JSON for `--diff` obeys the same real limitation as #096:
   until `dispatch_format()` becomes orthogonal to the run mode, the diff path has no machine-readable output.
7. Tests:
   parsing/aggr — new `tests/unit/git/diff_query_test.cpp`;
   rule logic — new `tests/unit/scan/test_co_evolution_test.cpp`;
   repo-level smoke — 1-2 scenarios in [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/test_co_evolution/pass/with_tests.numstat`
- `fixtures/test_co_evolution/fail/prod_without_tests.numstat`
- `fixtures/test_co_evolution/pass/docs_only.numstat`
- `fixtures/test_co_evolution/pass/generated_or_vendor_ignored.numstat`

Mandatory checks:

- production vs test churn is counted correctly;
- a docs-only diff produces no finding;
- a PR with sufficient test churn produces no finding;
- the message contains production churn, test churn and ratio/threshold;
- the output is compatible with the current text/json contract.

## Readiness criteria

- Advisory-only by default.
- The current test/vendor/generated classification is reused where possible.
- No new config schema block within this task.
- The signal does not mask structural diff findings and does not break their exit semantics.

## Do not do

- Coverage analysis.
- Per-test impact analysis.
- An attempt to prove that "there are definitely no tests".
- Gate by default.

## Done

**Detector test_co_evolution:**
- `include/archcheck/scan/test_co_evolution.h` — detector header (public interface)
- `src/scan/test_co_evolution.cpp` — implementation:
  - `aggregateChurn()` — split paths into buckets (production/test/other), count churn
  - `shouldReportCoEvolution()` — check the two thresholds (strict: 80 lines + 0 tests, soft: 200 lines + <5% tests)
  - `detectTestCoEvolution()` — public function with a ViolationList output (0 or 1 element)
  - Existing functions are used: `hasProjectExtension()`, `pathHasTestDir()`, `isTestBasename()`, `pathHasVendoredDir()`, `isVendoredFile()`

**Integration in main.cpp:**
- Added include `#include "archcheck/scan/test_co_evolution.h"`
- Call after the SATD block in `runDiffFullPath()`: `archcheck::git::collectNumstat()` → `detectTestCoEvolution()` → output (advisory)
- Output format analogous to SATD: "test co-evolution (advisory):" + list of violations

**Fixtures in `fixtures/test_co_evolution/`:**
- `pass/with_tests.numstat` — production 80 lines + tests 25 lines (no finding)
- `fail/prod_without_tests.numstat` — production 100 lines + tests 0 (finding by strict threshold)
- `pass/docs_only.numstat` — only .md files (no finding, no project extension)
- `pass/generated_or_vendor_ignored.numstat` — vendor + rename-only (no finding)

**Unit tests in `tests/unit/scan/test_co_evolution_test.cpp`:**
- Parser for the .numstat format (raw tab-separated lines)
- 9 tests covering:
  - strict threshold: prod 100 + tests 0 → finding
  - insufficient tests: prod 100 + tests 20 → no finding
  - docs-only: no finding
  - rename-only: no finding, not counted in churn
  - soft threshold: prod 250 + tests 5 (2%) → finding
  - vendor exclusion: vendor files not counted as prod
  - mixed case: vendor + prod + tests
  - boundary case: exactly at the 5% ratio → no finding (< not <=)

**Integration test in `tests/integration/diff/git_diff_test.cpp`:**
- `test_co_evolution: production change without test update triggers detection`
  - git repo with src/main.cpp + tests/test_main.cpp
  - baseline commit, then a change of 85 lines in prod without changing the tests
  - check for the presence of TEST.1.prod_changed_tests_silent in the output

**CMakeLists.txt:**
- `src/CMakeLists.txt`: added `scan/test_co_evolution.cpp` to the `archcheck_core` target
- `tests/CMakeLists.txt`: added `unit/scan/test_co_evolution_test.cpp` to the `archcheck_tests` target

**Code quality:**
- clang-format: formatted (120 columns, Allman braces, 2 spaces)
- cppcheck: OK (no warning/error)
- lizard: 12 CCN, 23 NLOC (within the 15/30 limits)
- All 390 unit+integration tests pass
- archcheck self-check: src/, include/, tests/ give 0 violations (SF.7/8/9/21, cycles, god-headers, chain-length)

## In progress

- (empty)

## Next steps

1. Extract numstat parsing as a reusable helper.
2. Align production/test path classification with the already-existing logic in the repo.
3. Add fixtures on churn ratio.
4. Check the behavior on `archcheck` itself.

## Key decisions

| Decision | Reason |
|---------|---------|
| Advisory-only | This is a review signal, not proof of a bug |
| Reuse the current file classification | One must not breed divergent rules for "what counts as a test" |
| `file="<diff>"`, `line=0` allowed | numstat has no precise line anchor |
| A ratio heuristic, not a binary "tests exist/do not exist" | Otherwise the noise on small PRs would be too high |

## Planned files

| Area | Change |
|---------|-----------|
| `src/git/` or diff orchestration | Numstat parsing |
| `src/main.cpp` / cheap-drift pass | Wiring up the test-co-evolution signal |
| `tests/unit/` | Churn aggregation and classification |
| `tests/integration/` | Diff-based scenarios |
| `fixtures/test_co_evolution/` | `pass/` and `fail/` numstat fixtures |

## Outcome
**Status:** completed — numstat parsing, ratio of test/production churn,
advisory in `--diff` (commit cb6e09d: `src/scan/test_co_evolution.cpp` + unit tests +
`fixtures/test_co_evolution/`).
**Date completed:** 2026-06-11
