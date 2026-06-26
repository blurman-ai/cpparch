# [RULES][DRIFT] DRIFT.2 (cycle growth) → default blocking gate

**Created:** 2026-06-06
**Started:** 2026-06-06
**Completed:** 2026-06-06
**Status:** completed
**Module:** RULES/DRIFT
**Priority:** major
**Difficulty:** M
**Blocks:** —
**Blocked by:** —
**Related:** #009 (DRIFT rules), #082 (alignment umbrella), [docs/research/drift_signal_validation.md](../../docs/research/drift_signal_validation.md) (corpus proof)

## Goal

Make DRIFT.2 (growth/appearance of dependency cycles) a **default blocking gate**
(exit `1`), rather than just a report.

## Justification (by data)

Corpus validation ([drift_signal_validation.md](../../docs/research/drift_signal_validation.md)):
DRIFT.2 fires on **72 of 135,092 commits = 0.05%** across 310 repositories.
This is an exceptionally low false-alarm rate, and a new cycle is an objective architectural
regression (Lakos physical design). The combination "rare + objective" = ripe for a
hard gate. Ordinary linters don't catch this class.

## What's needed

1. Confirm the current DRIFT.2 behavior in `--drift-baseline` (it reports now; what
   exit code?).
2. Make DRIFT.2 blocking by default in drift mode (exit `1` on cycle growth).
3. Verify that pre-existing cycles (in the baseline) do NOT break things — we gate only **new/
   grown** cycles, not legacy.
4. Fixtures: `fixtures/drift_cycle_growth/` already exists — extend pass/fail for the gate semantics.
5. Document the exit contract across all layers (help/docs/CHANGELOG).

## Acceptance criteria

- [x] DRIFT.2 breaks the build (exit 1) on the appearance/growth of a cycle in drift mode.
- [x] Pre-existing cycles from the baseline don't cause a false fail.
- [x] Fixtures pass/fail cover the gate behavior (`drift_cycle_growth` + live exit verification).
- [x] The contract is reflected in help/docs/CHANGELOG.

## How it was done (2026-06-06)

**Finding at start:** `--drift-baseline` exited `all.empty() ? 0 : 1`, i.e. it failed
on **any** violation, including legacy SF/Lakos. On LibreSprite that's exit 1 because of 259
legacy, not because of drift → useless as a gate. That was the real problem.

**Fix** (`src/main.cpp` `applyBaselineAndReport`): in drift mode (`baseline.driftFile`)
the exit is determined **only** by the number of DRIFT.1/DRIFT.2 (regressions). Legacy intrinsic
(SF.*/Lakos.*) and advisory DRIFT.3 are reported but don't gate. The line
`drift gate: N gating regression(s) ...` is printed.

**Policy (justified by corpus validation):**
- **gating:** DRIFT.1 (new shortcut), DRIFT.2 (new/grown cycle) — precise, rare, objective.
- **advisory:** DRIFT.3 (noisy on restructure — see eyeball in `drift_signal_validation.md`),
  pre-existing intrinsic (that's debt, not a diff regression).

**Live verification** (LibreSprite):
- before `60eed0f` → after `276fdbd` (DRIFT.1=1): **exit 1** ✓
- baseline == current state (259 legacy, 0 drift): **exit 0** ✓ (used to be 1)

Help + CHANGELOG updated. Gate: clang-format/cppcheck/lizard clean, 347 tests, coverage PASS.

## Notes

- Don't touch DRIFT.1 semantics in this task (a separate signal).
- Don't gate cross-area raw (see validation — ~50% FP).
