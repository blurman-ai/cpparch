# [FIXTURES/DRIFT] Integration fixtures for DRIFT.1 and DRIFT.2

**Created:** 2026-05-28
**Started:** 2026-05-28
**Status:** completed
**Module:** FIXTURES/DRIFT
**Priority:** minor
**Complexity:** S (half a day)
**Blocks:** —
**Blocked by:** —
**Related:** #009 (ai_drift_regression_rules)

## Goal

Add fixtures and an integration test for `--drift-baseline` to verify the CLI pipeline of DRIFT rules end-to-end (loading the graph baseline from disk, scanning, reporting violations).

## Context

Unit tests for DRIFT.1/DRIFT.2 cover the rule logic. Fixtures are needed to verify the full pipeline: real `.h` files + a saved `baseline.graph.yml` + running `archcheck --drift-baseline`. The peculiarity of DRIFT fixtures: each contains not only sources, but also a pre-saved `baseline.graph.yml` (the "before" graph slice).

## Execution plan

- [x] Create `fixtures/drift_shortcut_edge/pass/` — files identical to baseline, no violations
- [x] Create `fixtures/drift_shortcut_edge/fail_new_coupling/` — `a.h` adds shortcut a→c on top of a→b→c
- [x] Create `fixtures/drift_cycle_growth/pass/` — cycle unchanged relative to baseline
- [x] Create `fixtures/drift_cycle_growth/fail_new_cycle/` — new cycle `a.h ↔ b.h`
- [x] Write integration test in `tests/integration/rules/drift_fixtures_test.cpp`
- [x] Register in `tests/CMakeLists.txt`

## Done

- All 4 fixtures created with hand-written `baseline.graph.yml` (format v1)
- `tests/integration/rules/drift_fixtures_test.cpp` — 4 tests, all green
- Added to `tests/CMakeLists.txt`
- Full suite: 213 test cases / 726 assertions — OK
- lizard clean

## In progress

- (empty)

## Next steps

1. Commit and move to `completed/`

## Key decisions

| Decision | Reason |
|---------|---------|
| baseline.graph.yml stored directly in the fixture folder | Fixture is self-contained, test does not generate baseline on the fly |
| pass/ = files match baseline | No changes → no DRIFT violations |
| fail_new_cycle triggers both DRIFT.1 and DRIFT.2 | b.h→a.h — new edge between two baseline nodes → both rules fire; test filters by ruleId |
| `build_graph_from_dir` extracted into a separate helper | lizard --length 30; run_drift_check would otherwise be 36 lines |

## Changed files

| File | Change |
|------|-----------|
| `fixtures/drift_shortcut_edge/pass/` | new (a.h, b.h, c.h, baseline.graph.yml) |
| `fixtures/drift_shortcut_edge/fail_new_coupling/` | new (a.h adds a→c shortcut) |
| `fixtures/drift_cycle_growth/pass/` | new (a↔b cycle, baseline matches) |
| `fixtures/drift_cycle_growth/fail_new_cycle/` | new (baseline a→b only; current a↔b) |
| `tests/integration/rules/drift_fixtures_test.cpp` | new — 4 integration tests |
| `tests/CMakeLists.txt` | added new test |

## Fixtures

- [x] `fixtures/drift_shortcut_edge/pass/`
- [x] `fixtures/drift_shortcut_edge/fail_new_coupling/`
- [x] `fixtures/drift_cycle_growth/pass/`
- [x] `fixtures/drift_cycle_growth/fail_new_cycle/`
