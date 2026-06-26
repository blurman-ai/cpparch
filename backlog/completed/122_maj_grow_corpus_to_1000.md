# [RESEARCH][CORPUS] Grow the corpus 481 → ~1000 for a unified run

**Created:** 2026-06-12
**Started:** —
**Closed:** 2026-06-26
**Status:** done (1188 repos, window 2024-06; unblocked #119/#134); the remainder re-measure of vendored repos after #127 is tracked in #131
**Module:** RESEARCH][CORPUS
**Priority:** major
**Complexity:** medium
**Blocks:** ~~#119~~ — dataset ready (`results_full.jsonl`), #119 unblocked
**Blocked by:** —
**Related:** #066 (part 2 of the download), #074 (discovery ROI), [[project_grow_corpus_audit]], [[project_corpus_criteria_gate]]

## Goal

Obtain a **clean ~1000-repo C++ corpus, measured uniformly** (new binary
#113/#114 + window 2024-06), for #119. This is NOT "grow from scratch by cloning" — almost everything
is already on disk; the work = clone the remainder + clean gate + full regeneration.

## Context (clarified 2026-06-12 after the inventory)

The real picture turned out different from "481 must be grown by cloning":
- In `oss/` there are already **1271 dirs**, of which **~1209 are C++ with .git**; **432** have a `*_graph_drift.jsonl`.
- **Those 432 measurements ARE STALE**: made with the old binary (before #113/#114) and window 2025-05.
  For a consistent #119 dataset they need to be **re-measured**, not just new ones added.
- **~490** C++ repos on disk passed the rough gates but weren't measured (interrupted catch-up).
- Worklist remainder (not on disk) = **317** pre-gated candidates → cloning gives headroom to 1000+.

So the work is three parts: (0) clone the remainder, (1) **a real gate** over the whole pool
(clean denominator), (2) **a full regeneration of all eligible** with one binary+window (overwriting
the stale jsonl). "C++ wedge" = applying the gate, NOT deleting non-C++ files (measured:
it frees ~1 GB, .git holds the weight — not worth a destructive pass over 1209 repos).

Tool: `grow_corpus.py` (idempotent; patched with `--shallow-since=2024-06-01`,
stop-by-count disabled), `grow_corpus_worklist.tsv`, `grow_corpus_ledger.tsv`.

## Acceptance criterion for additions (decided 2026-06-12)

**Hard gates — UNCHANGED** (as in `CORPUS_CRITERIA.md`):
- C++/C primary
- **>300 commits since 2025-05-01** (the main criterion, stays as is)
- Agentic: AI trailer OR bot ≥10% OR median msglen ≥100
- Not a fork, not a giant with strong review
- Pre-filter: fork/mirror/commits>8000; C++ content ≤50MB

**Soft ranking of the worklist (weight, NOT exclusion):**
- **Mixed authorship** (both agent- AND human commits in the window) — raise higher.
  Reason: the key agentic conclusion rests on within-repo comparison (#115 §8.4);
  born-agentic repos (ThemisDB) give no baseline. Weight only — not critical.
- **History depth ≥2 years** — desirable, not mandatory.
- **Maturity (history before 2025)** — desirable, but there are almost none → not mandatory.

**Clone:** `--shallow-since=2024-06-01` (for the 24-month analysis window of #119, not 2025-05).
Where 2-year history exists — it'll be pulled in; where not — less, and that's ok ("desirable,
not mandatory"). Removes the off-by-one conflict of the shallow boundary with the analysis window
(the class that bit lateral in #115).

## Execution plan

### Phase 0 — clone the remainder (IN PROGRESS)
- [x] Patch `grow_corpus.py`: `--shallow-since=2024-06-01`, stop-by-count disabled.
- [~] Catch-up run over the worklist remainder (317 candidates), detached
      `setsid nohup … </dev/null &` (window reload / harness kill it — [[project_grow_corpus_audit]]).
      grow_corpus filters cut off GIANT/TOOBIG/NOCPP/CLONEFAIL — junk won't get through.

### Phase 1 — a real gate (clean denominator)
- [ ] Apply the full gate to the WHOLE on-disk C++ pool (not just new clones):
      agentic + >300 commits since 2025-05 + not fork/mirror/giant + real C++.
      Fork/giant — via `gh api` (fork flag, contributors, size). → list of eligible.
- [ ] If eligible < 1000 — pull more from the worklist; the target is a flat **≥1000**.

### Phase 2 — full regeneration (handed to #119 Phase 0b)
- [ ] Re-measure **all** eligible (incl. the 432 stale) with the new binary + `--since=2024-06-01`.
      Overwrites the old jsonl. A long job — detach + monitor.
- [ ] Append to the ledger, record the final denominator for the report.

## Do not do

- Do NOT tighten the gate with mixed-authorship — it's weight only (user's decision).
- Do NOT overwrite the ledger — append (audit trail).
- Do NOT clone forks/mirrors/mega — the pre-filter stays.

## Key decisions

| Decision | Reason |
|---------|---------|
| Don't touch the hard gates | They work; the additions are a continuation, not a revision of criteria |
| Mixed authorship — weight, not a gate | Mature mixed repos are few; a hard criterion would zero out the additions |
| Clone shallow-since=2024-06-01 | Sync with the #119 analysis window; catches 2 years where it exists |
| 2-year history desirable, not mandatory | Such repos are few; a hard requirement won't reach 1000 |
| Re-measure the old 432 too (not just new) | Made with the old binary/window — stale; mixing old+new = apples-to-oranges |
| "C++ wedge" = the gate, NOT deleting files | Deleting non-C++ frees ~1 GB (.git holds the weight) — not worth being destructive over 1209 repos |
| Clone = top-up, not growth from scratch | ~920 eligible already on disk; we clone only the remainder up to a flat 1000 |

## Changed files

| File | Change |
|------|-----------|
| `analysis/agentic/grow_corpus.py` | `--shallow-since=2024-06-01`; soft ranking |
| `experiments/ai_repo_run/grow_corpus_ledger.tsv` | +newly reviewed repos |

## Done

- **Corpus grown and measured** (2026-06-18): **1188 C++ repos, 484,500 commits, window
  2024-06**, run of `archcheck --diff` per-commit. Result and analysis —
  `docs/research/agent_drift_within_repo.md`; data — `experiments/per_commit/results_full.jsonl`.
  Phases 0–2 (clone + gate + regen) are effectively done — the plan/"In progress" below are outdated.
- **Caveat on the binary:** the census was on the **pre-#129/#127** binary. Numbers on
  vendored/generated-bearing repos are skewed → their targeted re-measure on the post-#127 binary
  is moved to #131 (partial re-measure), this is NOT a repeated full regeneration.

## In progress

- (grow_corpus catch-up finished — the result of 1188 repos is recorded in the report)

## Next steps

1. Check against the ledger, fix shallow-since, launch the catch-up detached.

## RESULT (2026-06-26) — closed

Goal achieved: the corpus grown to **1188 repos** (window 2024-06), the dataset
`experiments/per_commit/results_full.jsonl` is ready and **unblocked the unified run #119**
and per-struct bool-accretion #134. The remainder — a partial re-measure of vendored repos after
excluding #127 — is not part of corpus growth and is tracked as part of the master verifier #131.

- **How it works:** `grow_corpus.py` + an audit trail (`grow_corpus_ledger.tsv` = reviewed/rejected).
- **What controls it:** the criteria gate `experiments/CORPUS_CRITERIA.md` (>300 commits since May 2025).
- **Related to:** unblocked #119/#134; the remainder re-measure — #131; the exclusion rule — #127.
- **Diagnostics:** corpus size = number of lines in the worklist; the ledger shows the reason a repo was rejected.
