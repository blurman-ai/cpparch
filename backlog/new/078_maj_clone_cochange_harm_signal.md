# [RESEARCH][DUPLICATION][DIFF] Co-change harm signal: clone importance by edit consistency over time

**Created:** 2026-06-03
**Started:** —
**Status:** new
**Module:** SCAN/DUPLICATION + DIFF
**Priority:** major
**Difficulty:** L
**Blocks:** severity ranking of duplicate findings
**Blocked by:** —
**Related:** #054 (ai_repo_duplication_run — git-history walk), #056 (token Type-3 core), #070 (precision), #071 (FP/TP classification)

## Goal

Give duplicate findings a **measure of importance (severity)** based not on textual
similarity in a snapshot, but on the **consistency of edits to the copies over time**. Right now the code
measures only structural similarity (`weighted`/`lcs`/`line`) — that is similarity, not
harm. Severity as a computed output **is absent** in the subsystem (grep
`priority|severity|importance` over `src/scan/duplication/` — zero).

## Root cause (literature)

The strongest empirical predictor of copy-paste harm is an **inconsistent change**:
one copy is edited, the sisters are forgotten. Bettenburg et al. (WCRE'09): "almost
every *unintentional* inconsistent change to a clone leads to a defect"
("clone consistency-defect"). At the same time a number of works (Krinke; Harder & Göde) showed
that clones can be more stable than ordinary code — ⇒ harm is **not universal**, and textual
similarity does not predict it. More detail and references:
[../../docs/research/code_clones.md](../../docs/research/code_clones.md) §2.

Conclusion: **a clone group's importance ≈ the frequency with which its copies were
changed inconsistently over history**, not their Jaccard today.

## Context: the infrastructure is already close

`#054` (diff-mode) already knows how to **walk git history without checkout** and attribute
copy-paste to a commit. This is the natural place to compute, for a clone group:

- how many times the copies changed in **one** commit (consistent co-change);
- how many times **one** copy changed while the sisters did not (inconsistent → risk);
- the ratio inconsistent / total → severity weight of the pair/group.

That is, what is needed is not a new backend but a layer on top of the existing history walk.

## Scope (proposal)

1. Clone group = transitive closure of pairs above the gate (not just pairs) — pair
   clustering into groups is needed (right now the report is pairs only; see the gap in the research doc §5.2).
2. From history: for each group compute a co-change matrix → `inconsistencyRatio`.
3. Collapse into a severity label (HIGH/MED/LOW), **orthogonal** to the similarity metric
   (mem:`fp_classification_rules`: "similarity-score ≠ refactor-priority").
4. Surface severity next to `type` in the report (the type is already produced — see #056 wire-in).

## Acceptance / fixtures

- A fixture with history: a group with consistent co-change → LOW; a group where one copy
  was fixed and the sisters went stale → HIGH.
- Determinism (CI contract): the same history → the same severity.

## Boundaries / risks

- Expensive: requires a history walk — justified only in diff/baseline modes, not in
  snapshot. Keep it behind a flag, like all of #054.
- Renamed/moved files over history (P0.2 already partial) — co-change is easy to
  over/underestimate on a rename; follow-rename attribution is needed.
- YAGNI check: introduce only if severity actually **changes the author's action**
  (what to fix first), not as yet another number in the report.

## Related / links

- Research summary: [../../docs/research/code_clones.md](../../docs/research/code_clones.md)
- Bettenburg et al. (WCRE'09): <https://users.encs.concordia.ca/~shang/pubs/bettenburg-wcre09.pdf>
</content>
