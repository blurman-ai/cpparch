# [DOCS][CLI] Sync help/README with the real CLI contract after the advisory-first wave

**Created:** 2026-06-23
**Started:** 2026-06-23
**Status:** completed
**Module:** DOCS / CLI
**Priority:** critical
**Difficulty:** S
**Blocks:** the public README/help as a trusted contract for the first run and CI
**Blocked by:** #141 (centralize_gate_policy) for the code part (help) — the gate contract is better described after a code-level SSOT, otherwise help will diverge again from a scattering of string checks. The README part can go in the docs cluster (#139 → #137 → #136)
**Related:** #044 (docs_readme_sync_shipped), #045 (docs_sync_roadmap_mvp_spec), #075 (mvp_v1_trusted_diff_workflow), #118 (drift4_lateral_rule), #133 (first_run_noise_floor)

## Goal

Bring `--help`, `README.md`, and the CI-facing CLI description into line with what the binary actually does today: what gates, what is advisory, and which modes even exist.

## Context

After the `#075/#096/#097/#101/#118/#123/#133` wave, the code and the user-facing text drifted apart again.

- `--drift-baseline` in help and README is still described as a gate only for `DRIFT.1/DRIFT.2`, but the code already gates `DRIFT.4.CYCLE` too:
  [src/main.cpp#L31-L47](../../src/main.cpp#L31-L47),
  [src/cli/check_command.cpp#L74-L87](../../src/cli/check_command.cpp#L74-L87).
- Plain `check` after `#133` is advisory-first: exit 1 only on `SF.9`, while `SF.7/SF.8/Lakos.*` stay advisory.
  This is fixed in the code and changelog, but in README it still reads like a more general
  "non-zero on failure" without a clear contract:
  [src/cli/check_command.cpp#L90-L107](../../src/cli/check_command.cpp#L90-L107),
  [CHANGELOG.md#L76-L89](../../CHANGELOG.md#L76-L89),
  [README.md#L23-L35](../../README.md#L23-L35).
- `--diff` has long been broader than "graph regressions only": besides the graph it already prints SATD,
  test co-evolution, local complexity, new clone drift, and flag-argument drift:
  [src/cli/diff_command.cpp#L143-L211](../../src/cli/diff_command.cpp#L143-L211).
  In README this is reflected partially and unsystematically.
- Help already has `--history`, but the public README narrative around it is almost absent:
  [src/main.cpp#L36-L42](../../src/main.cpp#L36-L42).

Bottom line: right now, from `README` and `--help` alone, the user cannot reliably tell
which signals break CI and which are merely advisory.

## Execution plan

- [ ] Capture the actual matrix of CLI behavior: mode, exit semantics, gating/advisory, output format.
- [ ] Update `src/main.cpp::print_help()` so that `--drift-baseline` and `check` describe the current gate contract.
- [ ] Rewrite the `README.md` sections `What it does`, `Quick start`, `Status` under the advisory-first model.
- [ ] Check `docs/ci_integration.md` and the example workflow: is there any old "any violation = fail" contract there?
- [ ] Finish off the e2e/CLI tests for the help text and status wording, so the next wave doesn't silently drift again.

## Done

- `src/main.cpp::print_help()` synced with the gate/advisory contract of check, drift, and diff.
- README rewritten under the advisory-first model and the check JSON `gate` / `disposition`.
- `docs/ci_integration.md` describes `--diff` as a gating verdict + advisory blocks, JSON as shipped, SARIF as future.
- The CLI E2E checks the help text for `SF.9`, `DRIFT.4.CYCLE`, and the diff gate wording.

## In progress

- (empty)

## Next steps

- None. When changing the gate policy, first change `rules/gate_policy`, then help/docs/tests.

## Key decisions

| Decision | Reason |
|----------|--------|
| Edit help and README together | it's one user contract; partial syncing goes stale quickly |
| Describe gate/advisory explicitly for each mode | the formula `exit non-zero on failure` is already insufficient and misleading |
| Check the CI docs too | that's exactly where the old contract hits user expectations hardest |

## Changed files

| File | Change |
|------|--------|
| `src/main.cpp` | update the `--help` text under the actual gate/advisory rules |
| `README.md` | sync the public CLI description |
| `docs/ci_integration.md` | align the CI narrative with the advisory-first model |
| `tests/integration/cli_smoke_e2e_test.cpp` | lock the help/status contract with a test |
