# [RESEARCH][SCAN] Validation and hardening of the checker on per-commit reports (iteratively ≤5)

**Created:** 2026-05-31
**Started:** 2026-05-31
**Status:** wip
**Module:** RESEARCH / SCAN / RULES
**Priority:** major
**Difficulty:** large
**Related:** #056 (token-dup detector), #070 (precision filters), #054 (AI-repo dense corpus), #053 (line-dup)

## Goal

Bring the deduplicator (#056) and archcheck to a **trusted** state on real
data: run them over suspicious commits of the dense corpus, produce detailed
per-commit reports, **verify 200 by eye**, write a verification report
(confirmed / bogus by FP class), open fix tasks for found FPs,
implement them, re-run. **Loop ≤ 5 iterations.**

## Context

The dense corpus (#054): study-grade C++ repos with >50% agentic commits after May 2025
(283 repos, `experiments/ai_repo_run/study_grade_all.tsv`), clones in
`~/oss/_aidev_dense/`. We do NOT yet trust the checker as ground truth — #056/#053 have
known FP classes (idiom, header↔impl, data-table, coincidental, vendor).

`#056 --diff <A>..<B> --repo <path>` gives per-commit attribution:
`commit=<sha> added_frags=N partial_hits=M` + `ADDED file:line <-> BASE file:line` pairs
(a duplicate born in the commit = a missing-reuse edge). This is exactly the "suspicious commit."

## Iteration protocol (repeat ≤5)

1. **Generation:** #056 --diff (+ archcheck drift-baseline where applicable) over
   agentic commits of dense repos → per-commit reports with `file:line` ADDED/BASE.
   Accumulate ~250-300 reports (with a margin for a sample of 200).
2. **Eye-verify 200:** open ADDED and BASE at the commit, a verdict for each:
   TP (real copy-paste/drift) / FP + class (idiom / header↔impl / data-table /
   coincidental / generated / vendor / segmentation-artifact).
3. **Verification report:** what confirmed, what was bogus, FP share by class,
   precision estimate. File `VALIDATION_ITER<N>.md`.
4. **Fixes:** for the dominant FP classes — separate tasks (#061+), implement them
   in #056/archcheck (observing code_quality: ≤30 lines/function, ≤50 lines/commit),
   rebuild.
5. **Repeat** with the same commit sample → compare precision before/after. Stop on
   reaching acceptable precision or 5 iterations.

## Execution plan

- [ ] Per-commit report generator (#056 --diff over dense, bounded range) → JSONL+md
- [ ] Iteration 1: 200 eye-verifications → VALIDATION_ITER1.md (baseline precision)
- [ ] FP classes → fix tasks (#061+)
- [ ] Implement fixes, rebuild #056/archcheck
- [ ] Iterations 2..5: re-run the same sample, precision before/after, stop criterion
- [ ] Final report: precision dynamics across iterations, what remains

## Acceptance criteria

- [ ] ≥200 commits verified by eye, verdicts recorded with justification
- [ ] Verification report by FP class is reproducible (commands + version of #056)
- [ ] Fixes tied to specific FP classes, precision gain measured
- [ ] All findings are claims with `file:line`+SHA, nothing is deleted on the checker's verdict

## Done

- **iter1:** eye-verified 200 per-commit pairs (10 agents, reading real code)
  → `--diff` precision = **16.5%** (33 TP). FP taxonomy: coincidental 46%,
  other 17% (renames), segmentation 15%. → `VALIDATION_ITER1.md`.
- **iter2:** cheap correct fixes — #061 (rename `-M`) + #064 (vendor-excludes).
  Minor (rename pairs are copies, not git-renames; vendored 1.6%). Commit `0beb2f7`.
- **iter3-4:** 4 architecture-first surface discriminators (content, cross-file,
  line, essence-LCS) **ALL refuted on labeled data** (calibration BEFORE
  re-verification). Root cause: all metrics are fooled by a single normalization — coincidental
  is "more copy-like" than a genuine renamed copy. The #056 code rolled back to a clean state (no slop).
  Commits `81d2c8c`, `40e036d`. → `VALIDATION_ITER3.md`.

- **Corpus eye-verification (#067, 2026-06-02):** workflow `verify_findings.js`,
  agents verified **135 drift repos** against real code (drift mode: drift over history,
  not `--diff`). Confirmed: **87/135 repos, 195 cases** of copy-paste; false **406** →
  **precision ≈ 32%**; confirmed include cycles **21** (in authored src). FP classes:
  idiom 189, coincidental 99, whole-file 51, other 31, data-table 21, generated 9, etc.
  Full breakdown → `experiments/ai_repo_run/VERIFICATION_REPORT.md`. FP fix proposals
  (405 from agents) moved to **#070** for a separate review.
- **round2 (extended coverage, #066, 2026-06-02):** 135 repos, commits 3× (exclude round1) +
  cycle archaeology. **+143** new copypaste, **+197** FP → totaling 338/603, **precision ≈ 36%**.
  Main new thing: **28 cycle-intro commits** (when/by what an include cycle was introduced). Dating revealed
  a split: AI-era drift (Silencer/flox/FlashCpp/Astroray/SparkEngine/kyotodd) vs pre-AI legacy
  (BALL 2005, CppKAI 2016, LLL-TAO 2019) vs vendor (Qt Creator, ggml). → `VERIFICATION_REPORT_R2.md`.
  New fixes carried into #070 (602 total).

**Conclusion:** #056 surface = RECALL stage; `--diff` precision 16.5%, drift mode 32% —
both low, coincidental is inseparable by thresholds (proven). Precision = the **semantic
confirm layer (#070)** — it's what produced the labeled data, i.e. it works; it needs to be built in as
a gate. Branch: `feat/060-checker-hardening` (not pushed), code diff = 15 lines (#061+#064).

## In progress

- Implementing the confirm layer (#070) as a pipeline stage — the next step.

## Changed files

| File | Change |
|------|--------|
| `experiments/ai_repo_run/per_commit_*.{sh,jsonl,md}` | generator + per-commit reports |
| `experiments/ai_repo_run/VALIDATION_ITER*.md` | verification reports by iteration |

## Outcome
**Status:** completed — the validation loop ran fully (iter1–4):
- iter1: eye-labeling of 200 pairs → precision --diff 16.5%, FP taxonomy
  (coincidental 46% / other-renames 17% / segmentation 15%) → VALIDATION_ITER1.md;
- iter2: cheap fixes #061 (-M) + #064 (vendor) — committed (0beb2f7);
- iter3–4: 4 surface discriminators refuted on labeled data BEFORE
  re-verification (a negative result is also a result, recorded);
  the #056 code rolled back to a clean state.
Heirs: FP-class proposals → #070; token precision ceiling → #083.
Today's postscript (#107): the loop's eye methodology grew into
disagreement-triage over external oracles.
**Completed:** 2026-06-11 (in fact — earlier; closed during a backlog pass)
