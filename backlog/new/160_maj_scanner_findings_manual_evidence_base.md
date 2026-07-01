# [SCAN][RESEARCH] Manual evidence base for scanner findings before public push

**Created:** 2026-07-01
**Status:** new
**Module:** SCAN / DIFF / RESEARCH
**Priority:** major
**Complexity:** M
**Blocks:** public confidence in advisory scanner output
**Blocked by:** —
**Related:** #158 (clone FP reduction), #159 (composition research), #161 (bool drift audit),
#162 (local complexity audit), #103 (copy-paste precision), #119 (unified per-commit drift)

## Goal

Build a documented, eyes-on evidence base for what `archcheck` actually catches today:

- representative true positives;
- representative false positives;
- ambiguous but useful advisory findings;
- known noisy classes that should stay open as follow-up tasks.

This is not a new rule implementation task. It is the final manual audit layer before a public
push: the project needs real code examples and defensible FP/TP judgement, not only aggregate
numbers.

## Scope

Cover the scanner/advisory findings that can appear in `--diff` or scanner-driven checks:

- `DRIFT.NEW_CLONE`;
- `DRIFT.BOOL_FIELD_ACCRETION`;
- `DRIFT.LOCAL_COMPLEXITY`;
- SATD/test co-evolution/flag-argument if they appear in the same sampled commits;
- duplication `--duplication` snapshot findings where they are relevant to #158/#159.

Deep dives for bool drift and local complexity are split into #161 and #162. This task is the
cross-signal index and final narrative.

## Required output

Create or update a documentation artifact, for example:

```text
docs/research/scanner_findings_manual_evidence.md
```

The document must contain:

- methodology: repos/commits selected, commands used, binary/version/commit hash;
- a table of at least 50 findings reviewed by eye;
- per-finding verdict: `TP`, `FP`, `MIXED`, `USEFUL_ADVISORY`, `UNKNOWN`;
- root-cause class for every FP;
- at least 20 full code examples, with both before/after or both clone sides when applicable;
- a short public-facing conclusion: which signals are reliable enough to show, which are
  advisory-only, and which still need follow-up.

## Sampling plan

Use a mix of:

- recent corpus runs already in `experiments/`;
- targeted local runs on `~/oss` repos;
- examples from completed validation tasks (#103, #134, #145, #158);
- fresh runs for any signal whose examples are stale or too synthetic.

Do not rely only on labels. For every example, open the actual code.

## Acceptance

- [ ] A research doc exists with commands, sample selection, and verdict table.
- [ ] At least 50 findings are manually classified.
- [ ] At least 20 real code examples are included or linked with enough context.
- [ ] Bool drift and local complexity rows link to #161/#162 results.
- [ ] All material FP classes either have an open task or are explicitly accepted as advisory noise.
- [ ] No product code is changed unless a concrete bug is found and covered by tests.

## Do not do

- Do not massage metrics without examples.
- Do not show cropped fragments that hide why a verdict was made.
- Do not mark a signal "public-ready" if the FP class is only guessed, not inspected.

