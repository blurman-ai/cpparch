# [SCAN][DRIFT] Local complexity drift manual FP audit and example catalog

**Created:** 2026-07-01
**Status:** new
**Module:** SCAN / DIFF / LOCAL_COMPLEXITY
**Priority:** major
**Complexity:** M
**Blocks:** #160 final scanner evidence base
**Blocked by:** —
**Related:** #101 (LCX product signal), #102 (prototype), #109 (corpus validation),
#160 (scanner evidence base)

## Goal

Manually verify what `DRIFT.LOCAL_COMPLEXITY` catches in real commits and produce a durable
example/FP catalog. This is the final eyes-on confidence pass for the most semantically subtle
scanner signal.

## What to verify

For each sampled finding, inspect the actual before/after function body and classify:

- **TP:** added branching/nesting/control-flow complexity in the changed function.
- **USEFUL_ADVISORY:** mechanically true complexity growth, but lower review value
  (generated-ish feature expansion, large one-shot implementation, deliberate parser table).
- **FP:** function matching error, rename/move artifact, preprocessor branch artifact,
  vendor/generated/test leak, lambda/body parsing bug, or volume mistaken for complexity.
- **MIXED:** commit has both real LCX findings and noisy findings.

## Data sources

Use:

- #109 artifacts and notes (`experiments/lcx_corpus_run/`, if present);
- current product binary via `archcheck --diff`;
- `~/oss` local clones;
- completed #101/#102 research docs for known previous FP classes.

Prefer current product output over old prototype output.

## Required output

Create or update:

```text
docs/research/local_complexity_manual_audit.md
```

Include:

- commands used;
- sample-selection method;
- at least 40 manually reviewed LCX findings;
- at least 15 full before/after function examples;
- FP taxonomy with concrete examples;
- comparison against known #109 FP classes: overload cross-match, deletion shift,
  arity-change fallback, unparseable parent, preprocessor branch artifacts, lambda parsing,
  vendored version dirs, rescope/move/rename cases;
- final recommendation:
  - keep as-is;
  - tune reporting/wording;
  - add suppression/fix tasks;
  - or downgrade visibility.

## Checks to perform

- [ ] Confirm volume-only additions do not fire as complexity growth.
- [ ] Verify moved/renamed functions do not appear as new complexity unless there is real growth.
- [ ] Check overloads and same-name functions manually.
- [ ] Check preprocessor-heavy files and lambda-heavy files.
- [ ] Separate "real but low review value" from actual FP.
- [ ] Record examples that are good enough for public docs.

## Acceptance

- [ ] 40+ findings manually classified.
- [ ] 15+ real before/after examples recorded.
- [ ] Precision estimate and FP taxonomy written down.
- [ ] Any concrete product bug found has a follow-up task or a fix with tests.
- [ ] Results are linked from #160.

## Do not do

- Do not trust the score delta without opening the function body.
- Do not count a moved implementation as complexity growth unless the body itself changed.
- Do not change the cognitive-complexity scorer without adding focused regression tests.

