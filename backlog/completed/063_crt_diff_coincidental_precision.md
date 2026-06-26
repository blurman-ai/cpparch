# [SCAN] #056: precision against coincidental-FP (the main class)

**Created:** 2026-05-31
**Status:** completed (resolved — the surface-level path is impossible, handed off to #070)
**Completed:** 2026-06-05
**Module:** SCAN
**Priority:** critical
**Related:** #060, #056, #070

> **Outcome (resolved):** not solvable via thresholds/surface metrics — proven on
> labeled data (iter3): all three hypotheses refuted, coincidental matches look
> "more copy-like" than a genuine renamed copy by any surface metric ("idiom-FP floor").
> Precision comes only from a semantic confirm layer — implementation moves to **#070**.
> This task is closed as a negative result with proof, not as an achievement.

## Goal
Eliminate coincidental FPs (~46% in iter1, the MAIN class): two DIFFERENT functions match
because selective normalization collapses `id`/`lit` → different code yields a similar
token stream, and token-LCS pushes it to weighted=1.0 at low `line` (0.3–0.6).

## Hypotheses — ALL REFUTED on labeled data (iter3, VALIDATION_ITER3.md)
1. ~~content-overlap (callee/type Jaccard)~~ — for same-file (66%) it is high for both
   (shared class vocabulary). Does not separate.
2. ~~cross-file-only~~ — rejected: within-file copies (copied a function, edited
   part) are IN SCOPE, they must be caught.
3. ~~raw-line floor~~ — the `line` metric is normalized (deceived); on the data the
   distributions are inverted: FP line 0.88 > TP 0.67. A threshold drops TPs, keeps FPs.

**Root cause:** all surface metrics (token-LCS, norm-line, content) are deceived by the SAME
normalization — coincidental matches look "more copy-like" than a genuine renamed copy by them. The signal
is semantic. This is the "idiom-FP floor" from duplication_architecture.md §5/§9.

## Correct framing (architecture-first)
#056 surface = RECALL stage (16.5% precision). Precision comes from the **semantic
confirm layer** (#070 final): an agent reads both fragments → REAL/coincidental, gates.
Don't sharpen #056 with thresholds — proven. Next step: embed the confirm layer into the pipeline
as a gate on candidates that have passed the cheap filters.

## Status
The surface-level #063 is closed as a dead end (with proof). Moves to implementing the
confirm layer (#070).
