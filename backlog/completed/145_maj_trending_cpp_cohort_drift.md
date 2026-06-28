# [RESEARCH] Trending C++ cohort — drift descriptive run (separate cohort)

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** completed 2026-06-28 — 39 measured / 17 not-measured; first-look descriptive table done
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

- [x] Assemble the list of 56 (done: `cohort_trending56.tsv`, all trending).
- [x] 20 trending already cloned (5 local + 15 in the trending experiment); local-5 already
      scanned (`drift_local5.md`).
- [x] Clone the rest of the cohort.
- [x] Full per-commit drift scan of the whole cohort → `results_trending_cohort.jsonl`
      (14 991 commits, 56 repos).
- [x] Descriptive table + first look at the statistics → `drift_trending56.md`
      (39 measured / 17 not-measured), separate from the corpus.

## Result (2026-06-28)

**Done. Descriptive table built and corrected.** `drift_trending56.md` (+ `.tsv`), merged
data in `experiments/per_commit/results_trending_cohort_merged.jsonl`.

The first full scan (`results_trending_cohort.jsonl`, 14 991 commits) **blacklisted 27/56
repos as slow** (per-commit `archcheck --diff` is whole-tree-bound, see #149): their commits
came back as `skipped_slow_repo`, which the v1 table showed as near-zero — *not measured*
read as *clean*. Fixed with a **coverage gate**: a repo is *measured* only when scanned
ok-commits cover ≥60 % of its in-window commits; the rest are listed separately with no
drift claim.

Catch-up pass (`run_worklist.py`, timeout 180, never-blacklist, cheap-first) on the 12
under-cap repos recovered 11 → **39 measured, 17 not-measured**. Probe cost is *bimodal*:
a repo's non-C++ commits scan instantly, C++ commits rebuild the whole graph (PvZ probed
163 s/commit but its full 300-commit set finished in 7.5 min). The 15 high-star megarepos
(tensorflow/opencv/godot/…) exceed the timeout on every C++ commit and stay not-measured.

Artifacts: `aggregate2.py` (coverage-gated table), `merge_catchup.py` (catch-up overlay),
`results_trending_catchup{,2,3}.jsonl`.

**First-look headline:** the hard gate (new/grown cycle + new god-header) fired on
**17 / 10 032 commits = 0.17 %** across 9 repos; grown-cycles never. The lively advisories
are test co-evolution (median 9 %), local complexity (10 %), added edges (7 %). Descriptive
only — popularity-selected, never pooled into population estimates.

### Open / deferred
- The 17 not-measured repos stay not-measured by design (megarepo per-commit cost). If
  ever wanted, they need the #149 fix (scoped per-commit dup/graph scan), not more timeout.
- `Status: wip → ready to close` once the first-look is reviewed.

## How it works (completed summary)

Two-pass approach: (1) full scan `run_worklist.py` over all 56 repos → `results_trending_cohort.jsonl`; (2) coverage gate (`aggregate2.py`) marks a repo "measured" only when ok-commits ≥ 60 % of in-window commits; (3) catch-up pass (timeout 180, never-blacklist, cheap-first) recovers the borderline repos; (4) merge overlay (`merge_catchup.py`) → `results_trending_cohort_merged.jsonl`; (5) `aggregate2.py` final table → `drift_trending56.md` + `.tsv`.

## Key decisions

- **Coverage gate (≥ 60 %)**: prevents blacklisted repos reading as "clean" — a zero ok-count is "not measured", not "no violations".
- **Errors counted as not-measured** in `cov()`: a binary rebuilt mid-scan generates silent spawn-errors that can silently pass the 60 % gate if not counted — patched once caught on mame.
- **Convenience cohort, never pooled into corpus**: popularity-selected, labeled `cohort=trending`, separately reported, no population estimates.
- **17 not-measured megarepos stay not-measured**: they need O(diff) scan (#152), not a higher timeout.

## Changed files / artifacts

- `experiments/trending_run/drift_trending56.md` + `.tsv` — main output
- `experiments/per_commit/results_trending_cohort*.jsonl` — scan results + merged
- `experiments/trending_run/aggregate2.py`, `merge_catchup.py` — processing scripts

## Note

The trigger was a user question about sorting trending in mobile GitHub. Along the way
this branch confirmed bug #066 (6/6 trending CLONEFAILs came back to life with the right method) and
showed that trending (star-velocity) ⟂ our commit-velocity gate: a trending repo may
have <300 commits in the window (LightGBM — 167/176) and correctly NOT pass the corpus gate.
