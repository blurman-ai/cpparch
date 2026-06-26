# [REPORT][CLI] Add gate/advisory semantics to the check-mode JSON report

**Created:** 2026-06-23
**Start date:** 2026-06-23
**Status:** completed
**Module:** REPORT / CLI
**Priority:** critical
**Complexity:** S
**Blocks:** safe use of `archcheck --format json` in CI after the advisory-first check-mode
**Blocked by:** #141 (centralize_gate_policy) — the JSON reporter should receive a ready gate/advisory classification from a code-level helper, not hardcode string checks anew
**Related:** #055 (dogfood_json_escape_dedup), #075 (mvp_v1_trusted_diff_workflow), #133 (first_run_noise_floor), #136 (cli_docs_contract_after_advisory_wave)

## Goal

Make the JSON report of the regular `check` mode honest for CI: the JSON must explicitly say which findings gate the exit code and which are advisory.

## Context

After `#133` plain check-mode became advisory-first: exit 1 only on `SF.9`, while `SF.7`,
`SF.8`, `Lakos.GodHeader`, `Lakos.ChainLength` are printed but do not break CI:
[src/cli/check_command.cpp#L90-L107](../../src/cli/check_command.cpp#L90-L107).

Text-output explains this with the line `note: ... advisory finding(s) ... gate fails only on dependency cycles`.
JSON-output carries no such information:
[src/report/json_reporter.cpp#L11-L33](../../src/report/json_reporter.cpp#L11-L33).

The resulting conflict:

- the command `archcheck --format json <repo>` may return `exit 0`;
- yet the JSON contains `"violations": [...]`;
- inside the JSON there is no `gate`, `severity`, `advisory`, `gating`, or any other way
  to understand why violations exist but CI is green.

For a machine consumer this is worse than the text mode: there is a stable schema, but it lacks
the most important CI meaning.

## Execution plan

- [ ] Fix the check-mode JSON contract: top-level `gate: ok|fail` and/or separate `gating`/`advisory` lists.
- [ ] Don't break the existing schema version without need: if the shape changes, explicitly bump `version` or add backward-compatible fields.
- [ ] Plumb the gate classification from the CLI/report layer into `writeJsonReport`, without duplicating string checks all over the code.
- [ ] Add e2e: advisory-only fixture → JSON contains findings + `gate: ok` + exit 0.
- [ ] Add e2e: SF.9 fixture → JSON contains `gate: fail` + exit 1.
- [ ] Update README/CI docs as part of #136: machine-readable output cannot be described simply as a list of violations.

## Done

- `archcheck --format json` got a top-level `gate: ok|fail`.
- Each finding in the check JSON got `disposition: advisory|gating`.
- The JSON reporter uses `rules/gate_policy`, does not duplicate rule-id checks.
- E2E covers advisory-only JSON (`SF.7`, exit 0, `gate: ok`) and gating JSON (`SF.9`, exit 1, `gate: fail`).
- `CHANGELOG.md` records the additive JSON schema change.

## In progress

- (empty)

## Next steps

- None. On a future schema-breaking change, separately bump/discuss the JSON version.

## Key decisions

| Decision | Reason |
|---------|---------|
| JSON must carry the gate verdict | otherwise machine-readable CI output contradicts the exit code |
| Advisory findings stay in the report | hiding them from JSON is not allowed: the user would lose a useful signal |
| Don't copy classification into the reporter | the reporter should receive already-computed semantics, otherwise drift repeats |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/report/json_reporter.h` | extend the JSON report contract |
| `src/report/json_reporter.cpp` | add gate/advisory fields |
| `src/cli/check_command.cpp` | pass classification into the reporter |
| `tests/integration/cli/cli_smoke_e2e_test.cpp` | e2e for JSON advisory/gating semantics |
| `tests/unit/report/json_reporter_test.cpp` | a unit schema contract, if a separate test is appropriate |
