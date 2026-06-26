# [CLI][RULES] Centralize gate/advisory policy for check/drift/diff

**Created:** 2026-06-23
**Started:** 2026-06-23
**Status:** completed
**Module:** CLI / RULES / DIFF
**Priority:** critical
**Blocks:** robustness of the CLI contract: one source of truth instead of manual sync of code, help and comments; code prerequisite for #140 and code parts of #136
**Blocked by:** —

> **Priority raised major→critical (2026-06-23):** the task hard-blocks two `critical` tasks (#140, #136); a `major` prerequisite of two criticals is mislabeled. If it is decided that #140/#136 need not wait for the helper — revert to major and drop their block status.
**Related:** #075 (mvp_v1_trusted_diff_workflow), #118 (drift4_lateral_rule), #133 (first_run_noise_floor), #136 (cli_docs_contract_after_advisory_wave), #140 (check_json_gate_advisory_schema)

## Goal

Remove the smeared-out gate/advisory logic from various places in the code and set up one small code-level source of truth for which findings break CI in each mode.

## Context

Right now the gate policy is no longer simple, but lives in string checks and comments scattered across various places.

- Check-mode gates only `SF.9`:
  [src/cli/check_command.cpp#L90-L107](../../src/cli/check_command.cpp#L90-L107).
- Drift-baseline gates `DRIFT.1`, `DRIFT.2`, `DRIFT.4.CYCLE`:
  [src/cli/check_command.cpp#L74-L87](../../src/cli/check_command.cpp#L74-L87).
- Diff-mode gates grown cycles and new god-headers:
  [include/archcheck/diff/regression_report.h#L77-L79](../../include/archcheck/diff/regression_report.h#L77-L79).
- Help and headers have already fallen behind the code:
  [src/main.cpp#L31-L47](../../src/main.cpp#L31-L47),
  [include/archcheck/rules/rule_set.h#L15-L21](../../include/archcheck/rules/rule_set.h#L15-L21),
  [src/cli/check_command.h#L25-L31](../../src/cli/check_command.h#L25-L31).

This is exactly the mechanism that already produced `#136`: DRIFT.4.CYCLE was added to the code, but not to
all the descriptions. If the string checks are left scattered around, the next rule-wave will repeat the drift.

## Execution plan

- [ ] Introduce a small helper/type for classification: `gating` vs `advisory` per mode (`check`, `drift`, `diff`).
- [ ] Migrate `reportCheckGate` and `reportDriftGate` to this helper.
- [ ] Give the JSON reporter from #140 a ready-made classification, rather than making it know rule IDs.
- [ ] Update stale comments in `rule_set.h` and `check_command.h`.
- [ ] Add policy unit tests: `SF.9` gates check, `SF.7` advisory; `DRIFT.4.CYCLE` gates drift, `DRIFT.4.NEW` advisory.
- [ ] Make sure diff-mode is not mixed with rule-id policy: it has a structural `RegressionReport::gates()`, that is a separate branch.
- [ ] Separate in the helper and comments the NON-uniform gate: check/drift gate only cycles, while diff additionally gates new god-headers ([include/archcheck/diff/regression_report.h#L79](../../include/archcheck/diff/regression_report.h#L79)). Fix the lying comment [src/cli/check_command.cpp#L95](../../src/cli/check_command.cpp#L95) ("Mirrors the --diff/drift gating model (gate = cycles)" — for diff this is wrong). Centralization must not lock in a false symmetry (in sync with #139).

## Done

- Added `rules/gate_policy`: unified classification `gating` / `advisory` for `check` and `drift`.
- The `check` gate now centrally counts only `SF.9`.
- The `drift` gate now centrally counts `DRIFT.1`, `DRIFT.2`, `DRIFT.4.CYCLE`.
- Updated stale comments in `rule_set.h` and `check_command.h`.
- Added gate policy unit tests.

## In progress

- (empty)

## Next steps

- None. Diff-mode gate stays a separate `RegressionReport::gates()` over structural fields.

## Key decisions

| Decision | Reason |
|----------|--------|
| This is not a generic rule engine | only CI-severity classification is needed, not a new rule architecture |
| Keep diff-mode as a separate structural report | there the gate is determined not by ruleId but by structural fields of the report |
| Update comments together with the helper | stale comments have already become part of the problem |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/cli/` or `include/archcheck/rules/` | new minimal helper for gate/advisory policy |
| `src/cli/check_command.cpp` | use the helper instead of inline string checks |
| `include/archcheck/rules/rule_set.h` | update drift-rule comment |
| `src/cli/check_command.h` | update baseline/drift comment |
| `tests/unit/cli/` or `tests/unit/rules/` | policy unit tests |
