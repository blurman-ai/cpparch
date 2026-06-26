# [GRAPH] Reclone 297 corpus repos + full lateral run + normalized agentic breakdown

**Created:** 2026-06-12
**Started:** 2026-06-12
**Status:** done
**Module:** GRAPH][SCAN
**Priority:** major
**Difficulty:** medium
**Assignee:** Sonnet/Opus (NOT Haiku: network, volumes, statistical decisions)
**Blocks:** —
**Blocked by:** —
**Related:** #111 (lateral corpus run — covered 183/481), #109 (lcx corpus)

## Goal

Raise lateral-criterion coverage from 183 to ~481 corpus repos (re-cloning
297 that were removed from disk) and produce a normalized agentic vs human
breakdown (repo fixed effects) — 12 agent events across 183 repos is too few
to conclude.

## Context

Decision made 2026-06-12 ("we have to clone 2 and 3, there's no way around it").

- Run #111: 98 events across 183 repos, TP ≈ 87% — criterion is valid; the
  question of agentic differences stayed open due to power.
- List of the missing ones: `experiments/ai_repo_run/baselines/manifest.tsv`,
  rows where `status == repo_missing` (297 of them). GitHub name = `name` with
  the first `_` replaced by `/`.
- This is a **reclone of existing corpus members**, not a selection of new ones —
  the `experiments/CORPUS_CRITERIA.md` gate is NOT to be reapplied.
- Disk: ~79 GB free (2026-06-12), expected clone volume ~30–40 GB — it fits, but
  check `df -h` before start and after every hundred repos.
- Clone into `~/oss/<owner>_<name>` (flat scheme, like the rest).
  NOT shallow: shallow boundaries already cost 68 repos an off-by-one
  baseline (#111 §1). If a full repo clone is > 2 GB — clone with
  `--filter=blob:none`? NO: partial clones pull blobs over the network during
  ls-tree (observed in #111 — timeouts on GenieTim/pylimer-tools). Full clone
  or skip with a note in the manifest.
- Pipeline is ready and idempotent (`exists` in the manifest is skipped):
  `experiments/ai_repo_run/make_window_baselines.py` →
  `experiments/ai_repo_run/lateral_drift_scan.py`.

## Execution plan

- [ ] Reclone script driven by the manifest (sequential or 2–3 threads, retry
      on network errors, per-repo log); repos that vanished from GitHub
      (404/DMCA) — mark and skip.
- [ ] `make_window_baselines.py` — top up baselines for the new clones.
- [ ] `lateral_drift_scan.py` — full run; expect on the order of
      98 × (481/183) ≈ 250–300 events if density holds.
- [ ] Eyeball a fresh top-20 sample from the NEW repos (TP bar ≥ 70%, as in #111).
- [ ] Normalized agentic breakdown: denominator — share of agent commits in the
      repo over the window (git log + BOT_HINTS/Co-Authored-By, the classifier
      already exists in `lateral_drift_scan.py::classify_commit_author`), repo
      fixed effects — design as for boolean-drift.
- [ ] Augment `docs/research/lateral_module_drift_corpus_run.md` (full coverage
      + agentic result) — not a new doc, an update to the existing one.

## Done

- **Reclone (`reclone_missing.py`):** 297 `repo_missing` → full clones via
  `gh repo clone` (auth, no rate-limit), 3 threads, retry, cleanup of partial ones.
  Result: **296 cloned + 1 exists + 1 gone (404: studiocollective/songbird) + 0 failed.**
  Disk: 101 GB free afterward (consumed ~30 GB).
- **Baseline (`make_window_baselines.py`, idempotent):** 479 .yml.
  143 new `ok` + 183 prior + 151 `ok_empty` + 2 root. `ok_empty` checked —
  legitimate (window covers the repo's C++ history from inception; full clone
  removed the off-by-one #111). Backup of old manifest: `manifest.tsv.bak_before_reclone`.
- **Full run (`lateral_drift_scan.py`):** **495 events** (CYCLE 153 /
  SDP 58 / NEW 284) across 479 repos, 56 repos with events. The `exists` subset
  (183 repos) reproduced **exactly 98 events** — determinism confirmed.
  Old CSV saved: `lateral_drift_new.csv.bak_183repos`.
- **Eyeball top-20 from new repos (verification on the event SHA, not HEAD):**
  TP 17/20 = **85%**, CYCLE precision 11/13 = **84.6%** — bar ≥70% cleared.
  3 FP: 2 CYCLE misgrades (leaf target without back-edge) → task **#117**;
  1 NEW from a file-split.
- **Normalized agentic breakdown (`agentic_normalized.py`, repo fixed effects):**
  raw 294 agent / 201 human. Pooling (per 1k commits): agent 9.19 vs human 4.27
  = 2.15×. **But within-repo (50 mixed repos): 23 higher for agent / 27 higher for
  human, sign-test p≈0.67 — NOT SIGNIFICANT.** Pooling is a composition artifact:
  60% of agent events come from a single `makr-code/ThemisDB` (13,024 commits by
  copilot-swe-agent[bot] out of 18,818, classification correct). Conclusion:
  per-commit agent authorship does NOT raise drift; the increase is the
  agent-saturated tail of the repo distribution. Consistent with velocity-surge.
- **Report augmented:** `docs/research/lateral_module_drift_corpus_run.md` §8
  (full coverage, eyeball, agentic breakdown) + forward link from §4.

## In progress

- (empty)

## Next steps

- → **#117** — CYCLE grader: confirm back-edge B→A (2 FP from eyeball).
- Time-based grace period (30 days) — to be tuned if NEW noise becomes a problem
  during DRIFT.4 implementation; at advisory grade it does not block.

## Key decisions

| Decision | Reason |
|---------|---------|
| Full clone, not shallow nor partial | shallow → off-by-one baseline; partial → network blob fetches during ls-tree (#111) |
| Do not apply the CORPUS_CRITERIA gate | Repos are already selected into the corpus; this is restoration, not expansion |
| Update report #111, not a new doc | One source of truth for the lateral run |

## Changed files

| File | Change |
|------|-----------|
| `experiments/ai_repo_run/reclone_missing.py` | New reclone script (296/297) |
| `experiments/ai_repo_run/agentic_normalized.py` | New: normalization + repo fixed effects |
| `experiments/ai_repo_run/baselines/*` | +296 baselines (479 total) |
| `experiments/ai_repo_run/lateral_drift_new.csv` | Full run (495 events) |
| `docs/research/lateral_module_drift_corpus_run.md` | §8: full coverage + agentic breakdown |

## How it works

Reclone: `manifest.tsv` (status==repo_missing) → name `<owner>_<repo>` is inverted
into slug `<owner>/<repo>` (GitHub username contains no `_`, the first `_` is the
separator) → `gh repo clone` as a full clone. Normalization: one `git log --all`
per repo classifies the authors of all commits (the same BOT_HINTS + Co-Authored-By
as in the scan), intersected with the window from the jsonl → denominator of
agent/human commits; rate = events/commits; repo fixed effects = within-repo
comparison across 50 mixed repos (sign-test), which clears out between-repo
composition (the ThemisDB outlier).

## Completion date

2026-06-12

## Note on the commit

Run artifacts (`*.py`, `*.csv`, `baselines/`) live in the gitignored `experiments/` —
only the updated report `docs/research/...` and the task files go into the repository.
