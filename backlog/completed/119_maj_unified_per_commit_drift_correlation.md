# [RESEARCH][DRIFT] Unified per-commit drift dataset + correlation search

**Date created:** 2026-06-12
**Date started:** 2026-06-25
**Date completed:** 2026-06-25
**Status:** done
**Module:** RESEARCH][DRIFT
**Priority:** major
**Difficulty:** large
**Blocks:** —
**Blocked by:** — (the dataset is already ready: `experiments/per_commit/results_full.jsonl`, 1188 repos, window 2024-06 — see `docs/research/agent_drift_within_repo.md`; #122 grow+measure is de facto done)
**Related:** #111/#115/#117 (lateral), #103 (copypaste per-commit), #089/#090 (boolean state), #109 (lcx), #093/#094 (flag-arg/param accretion), #077 (per-commit export), #066 (corpus top-up)
**Note:** #119 depends on the DATASET of #122 (ready), NOT on #131. #131 (partial re-measure of vendored load-bearing repos) only refines them — immaterial for correlations. Can be run on the data at hand; the #119 work on the data = unfold `n_other` into lateral/coupling columns (the signals are already in the dataset) — see below.

## Goal

Bring ALL per-commit drift signals into one dataset (**1 row = 1 commit**) and
search for cross-signal correlations: does a "signature of a drift-prone
commit" exist at all, do the signals correlate with each other, and is anything tied to agentic
authorship — net of the compositional effect (#115 §8.4).

## Context

We want to measure drift AT COMMIT TIME. We wrote a pile of checks, but measured them
**one at a time** and on inconsistent data/windows. It's time to run everything together on
the same commits and see whether a correlation shows up.

The main open question of the whole wave: does a "drift-prone commit" exist as an
observable phenomenon, and is it distinguishable for agents? On lateral alone (#115) there's
NO per-commit agentic effect (composition). A joint run checks this on all signals
at once + searches whether some signals predict others.

## Precondition — full regen with all session features

The current jsonl/baselines/CSV were generated BEFORE the fixes and on the 2025-05 window. Without a regen
the signals are computed on dirty/different data:

| Feature | Where | In the data now |
|---|---|---|
| #113 Apache banner ≠ vendor | archcheck binary | ❌ Apache repos lost ~90% of files |
| #114 `-test`/`testutil` filter | archcheck binary | ❌ test code leaked into the graph |
| #117 lateral back-edge confirm | lateral script | ✅ already in the latest CSV |
| 24-month window `--since=2024-06-01` | `generate_per_commit_graph_drift.py` | ❌ nowhere |

## Execution plan

### Phase 0a — grow the corpus to ~1000 → **task #122** (precondition, decision B 2026-06-12)
The measurement corpus is currently = **481** repos; growth to ~1000 is moved out to a separate #122
(including resolving the conflict of the shallow boundary with the 24-month window). Grow BEFORE the regen,
so as not to run the expensive per-commit twice.

### Phase 0b — regen (precondition)
- [ ] Rebuild archcheck (picks up #113/#114).
- [ ] Regenerate `*_graph_drift.jsonl` for all ~1000 repos with `--since=2024-06-01`; baselines.
      This is expensive (per-commit `archcheck --diff`, ~hours–days, parallelize).

### Phase 1 — signals, key (repo, sha)
- [ ] graph: `added_edges`, `removed_edges`, `grown_cycles`, `new_area_deps` (jsonl)
- [ ] lateral: grade flag CYCLE/SDP/NEW (`lateral_drift_scan.py`)
- [ ] boolean: `new_bools` (`boolean_state/bool_history_scan.py`)
- [ ] local complexity: Σ Δcomplexity, max ΔCCN, arity growth (`lcx_corpus_run/`)
- [ ] duplication: new clones introduced by the commit (#103 — if ready; else gap)
- [ ] flag-args / param accretion (#093/#094) — INCLUDE if implemented, else mark the gap (`log` drop)
- [ ] `author_kind` (agent/human), `repo_kind`

### Phase 2 — join
- [ ] Wide table: `repo, sha, date, author_kind, repo_kind, <all signals>`.
      One row = a commit. Coverage = commits that passed ALL families (intersection);
      explicitly log how many and why dropped (no silent truncation).

### Phase 3 — correlation analysis (discipline mandatory)
- [ ] Pairwise correlations — **Spearman**, not Pearson (distributions heavy-tailed).
- [ ] **Control for commit size** (touched files/lines): almost all signals
      grow with size — this is a common driver. Compute partial correlation controlling
      for size, otherwise "everything correlates with everything" = artifact.
- [ ] **Repo fixed effects / within-repo** mandatory (otherwise Simpson — the #115 lesson).
      Check whether 1–2 outliers carry the correlation (top-share of contribution).
- [ ] **Verify every notable link case by case** on 3–5 commits
      ([[feedback_verify_each_case_over_aggregate]]) — real or artifact.
- [ ] Agentic slice — within-repo normalized, not pooled (as in #115).

### Phase 4 — report
- [ ] Correlation matrix; which links survive (a) size control (b) repo FE
      (c) case-by-case verification. Is there a "drift signature"? Agentic conclusion.
      → `docs/research/unified_per_commit_drift.md`.

## Don't do

- DON'T infer causation from correlation — observational, descriptive/advisory.
- DON'T show pooled correlations without within-repo and size control.
- DON'T silently cut commits at the join — log the drop and its reason.

## Key decisions

| Decision | Reason |
|---------|---------|
| Full regen — a precondition, not an option | Signals on different windows/dirty data are incomparable; #113/#114 change the graph |
| Spearman + size-control + within-repo | Without them a correlation = artifact of commit size and repo composition |
| Case-by-case verification of notable links | The standard of the drift-metric wave; the aggregate lies ([[feedback_verify_each_case_over_aggregate]]) |
| #093/#094 optional | Cheap-drift signals strengthen the matrix but don't block the run |

## Changed files

| File | Change |
|------|-----------|
| `analysis/graph_drift/generate_per_commit_graph_drift.py` | regen with the 24-month window (default already set) |
| `experiments/ai_repo_run/unified_drift_join.py` | new: join of signals into a wide table |
| `experiments/ai_repo_run/drift_correlation.py` | new: Spearman + size-control + within-repo + agentic |
| `docs/research/unified_per_commit_drift.md` | report: matrix + what survives the controls |

## Done (2026-06-25 — first full run)

**Preconditions (phases 0-2) removed by supersession:** the dataset is already wide — corpus run #090
(`results_full.boolrule.jsonl`, 517,975 ok-commits, 1188 repos) gives EVERY drift signal
as a separate column. The join (phase 2) is already built in; a separate `unified_drift_join.py` wasn't needed.
The regen (#113/#114/window) — the data is fresh (binary with the filters, June 2026).

**Phase 3 — correlations (`experiments/ai_repo_run/drift_correlation.py`):**
- [x] Base Spearman matrix of all signals.
- [x] Control for commit size (partial Spearman, control=`added_total`).
- [x] within-repo / repo FE (within-repo rank demeaning → partial).
- [x] Config-bag/outliers: bool×complexity holds at .12–.13 when dropping n_bool_field≥15/10/5/3.
- [x] Case-by-case check of bool×complexity (6 commits): 5/6 — bool and complexity in one module, flag→branching.
- [x] Agentic slice within-repo size-banded sign test (632 repos): not a single agent↑.

**Phase 4 — report:** [docs/research/unified_per_commit_drift.md](../../docs/research/unified_per_commit_drift.md).

**Verdict:** (A) there's no strong "drift signature", size is the universal driver; (B) a handful of weak-but-
robust structural couplings survive size+repo FE; (C) **bool_field × complexity** — a robust
coupling, confirmed case by case ("flag→branching in the same module", mechanism #089); (D) **there's no agentic
drift signature** (after within-repo+size no signal is higher for agents; with the labeling caveat).

**Follow-up 2026-06-25 (both follow-ups closed):**
- [x] `n_other` — **identically 0** across all 517,975 rows (the catch-all is empty: every archcheck signal
  is in a named column). Nothing to unfold — the note in the header is stale relative to the old binary, where
  bool violations leaked into n_other.
- [x] **Agentic relabel sensitivity:** full-sample human↑ — a labeling artifact (undeclared AI
  inflates the human bucket); in AI-saturated repos (agent≥30%, 356 repos) human↑ **fades across all
  signals**, the single complexity AGENT↑ shift (p=0.032) doesn't survive Bonferroni. Verdict D
  refined: **there's no robust agentic signature**, the direction depends on the authorship classification.
- The coverage boundary is locked in the report without silent truncation: lateral grade flags (#115) —
  a separate family, not in the binary's advisory output (n_other=0), not joined in here.

## In progress

- (empty)

## Next steps (optional, NOT blocking — task closed)

1. Join in the lateral grade flags (#115 `lateral_drift_scan.py` CSV) — an upgrade of the matrix with graph
   detail; on lateral the agentic conclusion is already in (dies under repo FE), unlikely to give anything new.
2. #134 (per-struct HISTORY) — a different axis, a separate "top struct accumulators" for the constraint-decay
   narrative; cheap from the 10,735 bool-commits of #090, without a full re-run.
