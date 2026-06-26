# [RESEARCH][SCAN] Overnight eyeball verification of drift findings (Workflow, 3:00)

**Created:** 2026-06-01
**Started:** 2026-06-02
**Status:** completed
**Module:** RESEARCH / SCAN
**Priority:** major
**Related:** #060 (validation), #056 (copy-paste), #054 (corpus), CORPUS_CHECK_REPORT.md

## Goal

Overnight (start 03:00, cron) verify by eye (with agents) the MAXIMUM of drift findings from
`corpus_check_summary.tsv` / `CORPUS_CHECK_DETAIL.md`. Turn "candidates" into
confirmed/FP with per-class precision.

## User requirements (exact)

- **Broad across repos:** guaranteed to enter EVERY drift repo (138 across the corpus)
  and verify **≥5 commits** there.
- **If budget remains — go deep:** add commits along the history, starting with the
  "fattest" repos (more history/findings).
- **Special interest:** transfer of CHUNKS of code (not the whole file) + **partial
  matches** (diverged/near within-file) + **cycles** (archcheck include cycles).

## Architecture (turnkey by 3:00)

1. `build_findings_worklist.py` → `verify_worklist.json`: for each drift repo
   pick ≥5 commits (priority within-file diverged/near = chunk-transfer/partial),
   + 1 cycle unit per repo with sccs>0; depth top-up by the fat ones.
2. Workflow `verify_findings.js`: pipeline over units → the agent reads ADDED@sha and
   BASE code, issues a verdict {real, class, evidence}; the cycle agent checks for a real
   include cycle. Budget — by agent count (breadth ≤5/repo guaranteed, then depth
   up to ~900 units; cap 1000).
3. Aggregate → `VERIFICATION_REPORT.md`: per-finding verdicts + per-class precision
   (within-chunk / partial / cross / cycle), per-repo, confirmed examples.

## Acceptance criteria

- [ ] Every drift repo: ≥5 commits verified (or all, if fewer).
- [ ] Cycles: every sccs>0 repo — cycle confirmed/refuted.
- [ ] Per-class precision computed; list of confirmed chunk-transfer/partial.
- [ ] **For EACH false positive: a description of "what fooled the detector" + a concrete
      fix proposal for #056/archcheck. We don't write an FP without analysis+fix.** Fixes
      cluster by FP class → a ready backlog of checker improvements.
- [ ] We don't trust the checker: the verdict is issued by the agent based on real code, claims with file:line+SHA.

## Launch
cron one-shot, durable, June 2 ~03:0x. The prompt is self-contained (see cron).

## Outcome

**Status:** completed
**Completed:** 2026-06-02..06 (actual); the file was closed 2026-06-11 following a backlog review.

The goal was achieved, but not in the planned form: instead of an overnight cron one-shot, mass
eyeball verification ran in daytime runs as part of #060 (round 1: 135 repos, 87 TP + 406 FP,
precision ≈32%; round 2: 66→135 repos, total 338/603, precision ≈36%, 28 cycle-intro commits).

Acceptance criteria closed in substance:
- per-class precision computed (see `experiments/verification/fp_corpus_r2_summary.json`);
- regression corpus assembled: `experiments/verification/fp_corpus_r2.tsv` (143 TP + 197 FP),
  per-finding verdicts in `experiments/verification/round2_verdicts/`;
- "each FP with analysis + fix proposal" → ~130 agent proposals, systematized
  and being implemented in **#070** (P0 guards + P1 classifiers already in code).

The task hung in `new/` after de-facto execution, because the work flowed down the #060/#070 channel,
and nobody closed the one-shot file. The results were consumed long ago — further work lives in #070.
