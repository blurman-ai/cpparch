# Does archcheck flagging predict bad outcomes? — a negative result

**Date:** 2026-06-19 · **Sample:** 30 repos, ~19.6 k commits (subset of the
per-commit corpus) · **Data:** `experiments/per_commit/results_full.jsonl` +
git history · **Probe:** `experiments/per_commit/outcome_linkage_probe.py`

## The question

The strongest possible value claim for a per-PR gate is **predictive**: *commits
the tool flags go on to cause real trouble.* So we tested it directly instead of
assuming it. A commit is **flagged** if archcheck fired any drift signal
(`grown_cycles | god_headers | cross_area | newclone | complexity > 0`); the
control is silent commits. Outcomes derived from git history:

- **re-fix / re-touch** — of the files a commit changed, ≥1 is touched again within
  the next 5 commits (a churn proxy; the "fix-flavored" variant filters to later
  commits whose subject looks like a fix);
- **revert** — the commit is the target of a later `This reverts commit <sha>`;
- **structural precedence** — does a clone/complexity fire *precede* a later
  cycle/god-header in the same repo.

## What we found

**The raw signal looks great — and is a size artifact.**

| measure | flagged | control | lift |
|---|---|---|---|
| re-fix (fix-flavored) | 39.5 % | 27.6 % | **1.43×** |
| re-touch (any) | 77.7 % | 64.0 % | 1.22× |

But **flagged commits are ~10× bigger** (median **200 vs 19** added lines, **5 vs 2**
files), and re-touch rises mechanically with the number of files a commit touches.
Control for it and the effect evaporates:

- **Within-repo, matched on size×file band** (30 repos): median Δ **+1.97 pp,
  p = 0.86** (re-fix); +2.67 pp, p = 0.58 (any). Directional lean 16+/14− repos —
  noise.
- **Mantel-Haenszel**, stratified by repo×size×files (more power than the 30-repo
  sign test): a residual **clone→re-fix OR = 1.19, p = 0.003**… but
  **leave-one-out kills it** — dropping a single high-churn agentic repo
  (`GregorGullwi/FlashCpp`, whose clone commits re-fix at 63.5 % vs 48.6 % control)
  collapses it to **OR = 1.07, p = 0.35**. The signal is one repo, not a corpus
  property.
- **Structural flags point the wrong way if anything:** cycle>0 OR = 0.68 (p = 0.42),
  god>0 OR = 0.44 (p = 0.12) — and far too sparse to trust (29 god + 34 cycle fires).
- **Reverts are useless as an outcome:** 48 total in ~19.6 k commits; flagged commits
  are reverted **less** (0.12 %, 3/2462) than control (0.27 %, 45/16943).

## Verdict

**No.** Once you control for commit size, archcheck-flagged commits are **not**
measurably more likely to be reverted or reworked. The 1.43× raw lift is the same
size confound that has bitten this corpus before (the "agentic velocity" effect that
died under repo fixed effects, and the per-commit size story in
[agent_drift_within_repo.md](agent_drift_within_repo.md)): **the difference is
volume, not a different kind of commit.** Reverts and structural-damage precedence
are too rare to test at this sample.

This does **not** say archcheck is worthless — its demonstrated value is
*differentiation* (it catches a category — new include cycles, god-headers,
cross-file copy-paste — that no mainstream CI tool models; see
`experiments/showcase/`), not *outcome prediction*. We just shouldn't claim the
gate "catches commits that later break" — the data doesn't support it.

## If someone wants to revive it

The honest bar to clear: a **full-corpus** (not 30-repo) within-repo, size+file
matched odds ratio that stays **> 1 at p < 0.001 after leave-one-out**. Reverts
won't carry it (too rare); a better outcome proxy would be *bug-fix commits that
later edit the same span* (needs commit-message classification), or linking to issue
trackers — both well beyond what git history alone gives. Until then, treat
outcome-linkage as **tested and not found**, not unexplored.

---
*Recovered from a workflow agent that computed this but crashed before emitting its
summary — the analysis was real, only the final hand-off failed. Numbers above are
from the agent's `answer.json` and run transcript, cross-checked.*
