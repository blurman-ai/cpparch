# [RULES/DRIFT] AI-oriented drift-regression rules

**Created:** 2026-05-26
**Started:** 2026-05-28
**Status:** done
**Module:** RULES/DRIFT
**Priority:** major
**Difficulty:** M (2-4 days for design, implementation in separate steps)
**Blocks:** —
**Blocked by:** ~~#008 (dependency_graph_foundation)~~ ✅
**Related:** #006 (spec_refactor)

## Goal

Specify and then implement the first, deliberately narrow prototype of
`drift-regression rules`, which proves the value of diff-based structural
analysis on `DRIFT.1` and `DRIFT.2`.

## Context

During a product discussion a new strong idea emerged: between zero-config
hygiene checks and a fully user-declared architecture policy there is an
intermediate class of rules — `drift-regression rules`.

Working name for the adjacent line of work where an agent reads the repository
and derives checkable architectural hypotheses: **AI-assisted rule synthesis**.

Their job is to answer not the question "what is generally wrong with you", but the question:

> which locally-convenient but globally architecture-eroding step was just taken in a change?

For the first prototype the scope is hard-narrowed to `DRIFT.1` and `DRIFT.2`.

## Execution plan

- [x] Fix `DRIFT.1` and `DRIFT.2` as the sole scope of the first prototype
- [x] Implement `DRIFT.1 no_new_shortcut_edge` + unit tests
- [x] Implement `DRIFT.2 no_new_cycle_or_cycle_growth` + unit tests
- [x] Fix the suppression mechanics: `--save-graph-baseline` → update the graph baseline
- [x] Bind the rules to the graph-baseline via the constructor (not to the violation baseline)
- [x] CLI: `--drift-baseline <file>` + `--save-graph-baseline <file>`
- [ ] Prepare fixtures (`drift_shortcut_edge`, `drift_cycle_growth`)
- [ ] Move `DRIFT.3+` into a separate follow-up task

## Done

- **DRIFT.1 `DriftNoShortcutEdge`**: a new edge between two files, both of which existed in the baseline. New files are not flagged. 6 unit tests, all green.
- **DRIFT.2 `DriftNoCycleGrowth`**: an SCC (cycle) grew or a new one appeared. Uses `graph::grownSccs()`. 5 unit tests, all green.
- **`makeDriftRuleSet(baseline)`** in `rule_set.h/.cpp` — factory for both DRIFT rules.
- **CLI**: `--drift-baseline <file>` runs the default rules + DRIFT; `--save-graph-baseline <file>` saves the include graph as a baseline.
- Full run: 713 assertions, 209 test cases — all green.

## In progress

- Fixtures for manual and integration testing (none yet)

## Next steps

1. Create `fixtures/drift_shortcut_edge/` and `fixtures/drift_cycle_growth/` (pass + fail)
2. Add an integration test that runs `--drift-baseline` on the fixtures
3. Open a `backlog/future/` task for the second wave of `DRIFT.3+`

## Key decisions

| Decision | Reason |
|---------|---------|
| DRIFT rules store the baseline in the constructor | Fits cleanly into `IRule` without changing the interface |
| DRIFT.1 flags only edges where both ends are in the baseline | New files may include anything — that is not a shortcut |
| Suppression = update the graph baseline via `--save-graph-baseline` | Consistent with the violation baseline pattern, no separate allow-list needed |
| `--drift-baseline` runs both the default rules and DRIFT | Single pass, full report |
| Severity: warning implemented via the standard `Violation` (no severity field) | The `ruleId` field is sufficient — add severity to `Violation` later if needed |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/rules/drift_no_shortcut_edge.h` | new — DRIFT.1 |
| `src/rules/drift_no_shortcut_edge.cpp` | new — DRIFT.1 impl |
| `include/archcheck/rules/drift_no_cycle_growth.h` | new — DRIFT.2 |
| `src/rules/drift_no_cycle_growth.cpp` | new — DRIFT.2 impl |
| `include/archcheck/rules/rule_set.h` | added `makeDriftRuleSet` |
| `src/rules/rule_set.cpp` | `makeDriftRuleSet` implementation |
| `src/CMakeLists.txt` | added two new .cpp |
| `src/main.cpp` | `--drift-baseline`, `--save-graph-baseline`, `applyDriftFile` |
| `tests/unit/rules/drift_no_shortcut_edge_test.cpp` | new — 6 tests |
| `tests/unit/rules/drift_no_cycle_growth_test.cpp` | new — 5 tests |
| `tests/CMakeLists.txt` | added two new tests |

## Fixtures (if a rule)

- [ ] `fixtures/drift_shortcut_edge/pass/`
- [ ] `fixtures/drift_shortcut_edge/fail_new_coupling/`
- [ ] `fixtures/drift_cycle_growth/pass/`
- [ ] `fixtures/drift_cycle_growth/fail_new_cycle/`

---

**Completed:** 2026-05-28

## How it works

Both rules implement `IRule` and store a copy of the baseline graph in the constructor:

- **DRIFT.1 `DriftNoShortcutEdge`**: calls `graph::addedEdges(baseline, current)`, filters edges where both ends (`from` and `to`) existed in the baseline. Flags each such edge — this is a "shortcut": an existing file added a new dependency on another existing file. New files are not flagged.
- **DRIFT.2 `DriftNoCycleGrowth`**: calls `graph::grownSccs(baseline, current)`, one violation per grown or new SCC ≥ 2.

## Controlled by

- `archcheck --drift-baseline <file> [path]` — runs the default rules + DRIFT.1/DRIFT.2.
- `archcheck --save-graph-baseline <file> [path]` — saves the include graph as YAML (suppression: save after review → new edges stop being "added").
- `makeDriftRuleSet(baseline)` in `rule_set.h` — factory, creates both rules.

## Related to

- `graph::addedEdges()` / `graph::grownSccs()` from `include/archcheck/graph/diff.h`
- `graph::loadBaseline()` / `graph::saveBaseline()` from `include/archcheck/graph/baseline.h`
- `IRule` from `include/archcheck/rules/i_rule.h`
- Fixtures and integration test — task #040

## Diagnostics

If DRIFT.1 does not flag an expected shortcut: check that both files actually exist in the baseline (`archcheck --save-graph-baseline` + inspect the YAML). If DRIFT.2 is silent on a new cycle: check that the cycle actually appeared only in current (`--diff HEAD~1..HEAD`).
