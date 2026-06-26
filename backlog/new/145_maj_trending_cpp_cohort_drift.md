# [RESEARCH] Trending C++ cohort — drift descriptive run (separate cohort)

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** wip
**Module:** RESEARCH / SCAN
**Priority:** major
**Related:** #066 (ai-stratum, same scan method), #119 (per-commit drift correlations), #115 (repo FE)

## Goal

Look at per-commit drift on **56 trending C++ repos** as a SEPARATE,
explicitly-labeled cohort — without the 300-commit gate, not mixing it with the corpus and not
claiming representativeness. We look at statistics on it **separately** (descriptive),
later.

## Sampling discipline (MANDATORY)

This is a **convenience-cohort**, NOT a representative sample (selection by popularity —
star-velocity / total stars). Therefore:
- it lives in a separate file/table with `cohort="trending"` and a `source` field;
- it is **never** pooled into population estimates and not merged into `results_full`;
- it is only good for descriptive "here is what's in these specific popular repos".
- do NOT apply the `>300 commits` gate (user's decision — take as-is).

Source of the discipline: the selection-bias discussion in the 2026-06-26 session (see also #066 §discipline).

## Cohort composition (56, `experiments/trending_run/cohort_trending56.tsv`)

**56 trending** — union of GitHub Trending C++ across daily+weekly+monthly (a single timeframe yields
only ~25, the union ceiling is ≈ 56 distinct). All `source=trending`. Top-starred is deliberately NOT
added (user's decision "no need for the old ones") — pure star-velocity, one definition,
without old mega-repos.

## Method (same as the #066 stratum — reusing the harness)

- Clone: `--no-checkout --filter=blob:none --shallow-since=2025-05-01 --single-branch
  --no-tags` + `GIT_LFS_SKIP_SMUDGE=1`, P≤4, timeout 300, 3 retries
  (`experiments/trending_run/clone_*.sh`). Clone AFTER the #066-377 clone (do not parallelize —
  rate-limit, that very bug #066).
- Scan: `experiments/per_commit/run_worklist.py`, window from 2025-05-01, no merges, no cap
  → `experiments/per_commit/results_trending_cohort.jsonl` (mark `cohort`/`source`).
- Aggregate: `experiments/trending_run/aggregate.py` → a separate md table.

## Plan

- [~] Assemble the list of 56 (done: `cohort_trending56.tsv`, all trending).
- [x] 20 trending already cloned (5 local + 15 in the trending experiment); local-5 already
      scanned (`drift_local5.md`).
- [ ] Clone the rest of the cohort (≈ 100 − already-cloned − intersection with #066-377).
- [ ] Full per-commit drift scan of the whole cohort → `results_trending_cohort.jsonl`.
- [ ] Descriptive table + first look at the statistics (separately from the corpus).

## Note

The trigger was a user question about sorting trending in mobile GitHub. Along the way
this branch confirmed bug #066 (6/6 trending CLONEFAILs came back to life with the right method) and
showed that trending (star-velocity) ⟂ our commit-velocity gate: a trending repo may
have <300 commits in the window (LightGBM — 167/176) and correctly NOT pass the corpus gate.
