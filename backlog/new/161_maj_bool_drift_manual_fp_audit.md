# [SCAN][DRIFT] Bool-field drift manual FP audit and example catalog

**Created:** 2026-07-01
**Status:** new
**Module:** SCAN / DIFF / BOOL_FIELD_DRIFT
**Priority:** major
**Complexity:** M
**Blocks:** #160 final scanner evidence base
**Blocked by:** —
**Related:** #089 (boolean state drift research), #090 (metric), #134 (full corpus run),
#135 (per-commit column), #160 (scanner evidence base)

## Goal

Manually verify what `DRIFT.BOOL_FIELD_ACCRETION` catches in real commits and build a documented
TP/FP example set. The question is not "does the parser run"; it is whether the signal is useful
and explainable to users.

## What to verify

For each sampled finding, inspect the actual before/after code and classify:

- **TP:** a type accumulates boolean state / mode flags / feature toggles in a way that suggests
  state-space growth or illegal-state risk.
- **USEFUL_ADVISORY:** config/options structs or UI state where the bools are real but less
  architectural.
- **FP:** parser/matching/classification bug, generated/vendor/test leak, rename/move artifact,
  or a field that is not actually newly accumulated state.
- **MIXED:** a commit with both legitimate boolean-state growth and noisy side findings.

## Data sources

Use:

- #134 outputs and `docs/research/boolean_state_perstruct_drift_fullcorpus.md`;
- native `archcheck --diff` runs for commits with `DRIFT.BOOL_FIELD_ACCRETION`;
- `~/oss` local clones.

If existing artifacts are stale, rerun a targeted sample rather than trusting the old report.

## Required output

Create or update:

```text
docs/research/bool_field_drift_manual_audit.md
```

Include:

- commands used;
- sample-selection method;
- at least 30 manually reviewed bool-drift findings;
- at least 12 full before/after code examples;
- FP taxonomy with concrete examples;
- final recommendation:
  - keep as-is;
  - tune reporting/wording;
  - add suppression/fix tasks;
  - or downgrade visibility.

## Checks to perform

- [ ] Verify generated/vendor/test exclusion on sampled findings.
- [ ] Check rename/move commits separately from real field accumulation.
- [ ] Separate config-bag bools from domain-state bools.
- [ ] Check whether repeated additions across several commits are more convincing than one-shot
      additions.
- [ ] Confirm the public message does not overstate the finding: it should say "added boolean
      state", not claim a bug.

## Acceptance

- [ ] 30+ findings manually classified.
- [ ] 12+ real code examples recorded.
- [ ] Precision estimate and FP taxonomy written down.
- [ ] Any concrete product bug found has a follow-up task or a fix with tests.
- [ ] Results are linked from #160.

## Do not do

- Do not count config/options bags as parser FP; classify them separately.
- Do not infer intent from names alone; inspect the surrounding type and commit diff.
- Do not change thresholds without a measured before/after and examples.

