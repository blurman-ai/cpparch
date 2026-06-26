# [SCAN][DUPLICATION] new-clone-gate: detect copy-paste introduced by a commit (--diff entry)

**Created:** 2026-06-13
**Started:** 2026-06-13
**Status:** wip
**Module:** SCAN][DUPLICATION
**Priority:** major
**Difficulty:** unknown
**Blocks:** —
**Blocked by:** —
**Related:** #103 (copypaste_per_commit_drift — research precursor, validates the thresholds), #117/#118 (diff_max_added_lines), #075 (trusted_diff_workflow), #052/#056 (token clone detector)
**Verification:** #131 (Group 2: stages 1+2 — precision/recall on the sample)

> **MVP:** pulled into v0.1 — a release blocker (decision 2026-06-13, [MVP.md](../../docs/MVP.md) §What-v0.1-is #6, §Acceptance #10). Reason: without it we ship archcheck with a validated detector locked in a chest, showing only the graph. Advisory-first; the gate (exit 1) stays v0.2.

> **Status 2026-06-23:** product path shipped as advisory: `DRIFT.NEW_CLONE`,
> parent-guard, local 10/10 control set and Catch2 E2E are in place. Remaining
> GitHub test repo is outward-facing validation/demo; either keep this task wip
> for that demo or split the demo into a follow-up, but core MVP behavior is no
> longer blocked here.

## Goal

Give `archcheck --diff` an entry point for duplicates: report copy-paste
**introduced by the given commit** (added-lines ∩ clone-spans), so it works
as a PR gate in CI — and not just a snapshot of the whole tree, as it is now.

## Context

**Why:** copy-paste and complexity growth are the canonical "smooth" degraders
(constraint decay, the project's whole reason): one diff is innocent, a year later the architecture
has rotted. Right now gating has only graph drift; copy-paste can't be gated, because
the clone detector **has no diff entry**.

**What exists (verified against the code 2026-06-13):**
- ✅ Token clone detector — `scanForDuplication` (shipped), but only a snapshot,
  called from `src/cli/preview_commands.cpp:126`.
- ✅ git-diff plumbing (old blob vs new blob, added-line ranges) — `src/cli/diff_command.cpp`,
  the same path that feeds LCX.
- ✅ The "old vs new → what was introduced" pattern — `analysis/local_complexity/scan_commit.py`
  (match → reason → finding); the per-commit one for complexity is already written.
- ❌ In `diff_command.cpp` there is NOT a single duplication call — `--diff` doesn't report
  copy-paste at all. That's the gap.

**Boundary with #103:** #103 is corpus research (Python, 185 repos, CSV, threshold
validation). This task is a product rule in shipped C++ that fails in CI.
#103 gives the thresholds, this gives the gate.

## Algorithm (minimal working version — what we converged on)

1. `git diff parent..HEAD` → **added-line ranges** per file
   (the plumbing already exists in diff_command).
2. Run `scanForDuplication` on the **new** tree → clone pairs (span A ↔ span B).
3. **Candidate finding** = a pair where at least one span **intersects the
   added lines** of the commit. Meaning: "the diff added code that is a clone
   of something else".

**FP guard (the only real one, see below):** drop pairs that were **already
clones in the parent tree**. Case: a duplicate X↔Y existed before the commit, the commit
only touched one copy (reformat) → its span intersected added-lines → a false "you
introduced copy-paste". Cured by comparing with the parent: a pair is "introduced" only if in the parent
it didn't match. This is a second scan (of the old tree) — more expensive, but more accurate.

**NON-FP (important, to avoid over-engineering):** "moved code" does **not** produce an FP.
If code really moved (deleted from A, added to B) — in the new tree it exists in a single
instance, there's no pair, nothing to catch. Rename protection isn't needed. If both copies
remained — it's real duplication, and it *must* fire.

## Progress (2026-06-13)

- ✅ **Basic detect** (commit `344870f`): `detectNewClones` + a branch in
  `diff_command.cpp` — added-lines ∩ clone-spans on the new tree, advisory.
- ✅ **parent-guard** (this session): a pair whose clone relation already existed in the parent is
  dropped. Pair identity = hash of the normalized `seq` of both sides →
  reformat/whitespace don't fool the guard; a fresh copy of pre-existing code still
  fires (in the parent there was one instance, no pair). 4 unit tests, dogfood 0,
  lizard clean. Implementation: a separate `scanForDuplication(parent)`.
- → **Validation moves to #124**: the product `archcheck --diff` over a sample of
  real clone-commits from the corpus (`--diff-mode=memory`, no checkout). The idea of an
  incremental corpus-walk was considered and **rejected** (a sample is needed, not every
  commit; the mechanism is redundant).

## Plan

- [x] `--diff`: add a duplication branch to `diff_command.cpp` (next to the LCX call)
- [x] Steps 1–3: intersection of clone-spans with the commit's added-line ranges
- [x] FP guard: parent scan, drop pairs that were already clones before the commit
- [ ] Severity: start **advisory** (like LCX), gating — a separate decision
      after corpus precision (#103/#124)
- [x] Reuse, don't rewrite: added-lines (diff_command path), `scanForDuplication`
- [ ] Fixtures (below)

## Validation (two-stage — the user's goal)

Don't replay the whole history of a real repo (expensive, thousands of runs). A
**curated set of 10 commits — 5 positives + 5 negatives** is enough.

**Precondition: the feature first** (the plan above) — without the duplication branch in `diff_command.cpp`
any run on copy-paste shows an empty report.

**Stage 1 — emulate CI locally. ✅ DONE (2026-06-20): 10/10.**
Run the same 10 commits through `archcheck --diff parent..commit` locally (the same as
CI will invoke on the runner). A fast loop on known ground truth.

Harness: `experiments/clone_gate_validation_123/run_control_set.py` (release binary;
base = the real monit-4.2 for realistic IDF + its pre-existing clones for the
parent-guard negative; a seed file of distinctive functions as ground truth). Each
scenario — an isolated branch off base, one commit, `--diff base..branch`.

| scenario | expectation | actual |
|---|---|---|
| P1 function copy into an existing file | FIRE | ✅ EXACT |
| P2 whole-file copy | FIRE | ✅ (4 pairs) |
| P3 large block | FIRE | ✅ |
| P4 exact copy | FIRE | ✅ |
| P5 renamed copy (moderate rename) | FIRE | ✅ |
| N1 unique new code | SILENT | ✅ |
| N2 move (code relocated) | SILENT | ✅ |
| N3 below the token threshold | SILENT | ✅ |
| N4 pre-existing clone touched (parent-guard) | SILENT | ✅ |
| N5 formatting-only | SILENT | ✅ |

Output: `DRIFT.NEW_CLONE — copy-paste introduced (EXACT): clone of <file>:<lines>`,
advisory (gate: ok). parent-guard and move-handling work correctly.

**Lesson about fixture design (important for future tests):** a renamed copy fires
ONLY if the function has distinctive (rare, df≤4) call names. If the seed functions
share common helpers → df grows → the rename loses fingerprint candidacy and a rare
common token → it goes silent (that's anti-FP §3, not a bug). Heavy-rename (every token) also
goes silent — P0.6 line-floor (#070/#059 tradeoff). A realistic rename positive = rare
calls + moderate rename. EXACT copies always fire via fingerprints (#092).

**Stage 2 — a test repo on GitHub. ⏳ REMAINING (outward-facing).** When it's clean locally — set up a test
repo, a workflow on a push trigger (not PR), the commit range via
`github.event.before..github.sha`:
```
archcheck --diff "${{ github.event.before }}..${{ github.sha }}" .
```
Each pushed commit = one CI run = one report. Run the control 10,
see how it looks live — a red check / sticky comment / step
summary. (It can also be via PR — then
[example_archcheck_pr.yml](../../.github/workflows/example_archcheck_pr.yml) as is.)

**Commit set** — picked to prove both recall and precision (the negatives hit the
FP classes). Source of positives: real copy-paste snippets from OSS, but assemble the
commits ourselves — plausible + ground truth known.

5 positives (must fire):
- a function copied into an existing file (the main Juergens case)
- a file-copy added in full
- a large block copied
- 2 more variations (EXACT / RENAMED)

5 negatives (must stay silent):
- unique new code
- **a move** (code relocated — no copy → move is not an FP)
- a duplicate below the token threshold
- **a pre-existing clone that the commit merely touched** (reformat of one copy) →
  tests the parent-guard, the most important negative
- a formatting-only change

**Success criterion:** all 10 behave as intended on the live CI surface (red
check/comment on 5, silence on 5).

## Next steps

1. ✅ diff entry + intersection of spans/added-lines (minimal version, advisory).
2. ✅ parent-guard against "touched an old duplicate".
3. ✅ Stage 1: 10-commit control set locally — 10/10 (2026-06-20).
4. ⏳ Stage 2 (outward-facing, needs supervision): a test repo on GitHub +
   a push-triggered workflow; assemble the same 10 commits, push them, read the reports
   in Actions.
5. ✅ (durable) Control set ported into a committed Catch2 E2E
   (`tests/integration/diff/diff_workflow_e2e_test.cpp`, tag `[newclone]`):
   synth base of 15 distinctive files (IDF doesn't degenerate, §3), 2 cases —
   positive (function copy → `DRIFT.NEW_CLONE`, advisory exit 0) + negative
   (unique code → silence). 549/549 green, dogfood 0, clang-format/lizard clean.
   Together with the unit tests (`new_clone_drift_test.cpp`: fires / parent-guard / outside
   / empty) — MVP criterion 7 ("fixtures per rule") for new-clone is closed.

## Key decisions

| Decision | Reason |
|----------|--------|
| added-lines ∩ clone-spans | the minimal "the commit introduced a clone" signal, all data exists |
| parent-guard against "touched an old duplicate" | the only real FP class |
| no rename-guard | move → no copy → no pair → the FP doesn't exist |
| advisory at the start, not gating | precision first on the corpus (#103), then the gate |
| product ≠ #103 | #103 research/thresholds; this — a shipped rule in CI |

## Changed files

| File | Change |
|------|--------|
| `src/cli/diff_command.cpp` | duplication branch in `--diff` |
| `include/archcheck/scan/duplication/*` | diff entry on top of `scanForDuplication` (TBD) |
| `tests/...` | unit + integration |

## Fixtures (if a rule)

- [ ] `fixtures/diff_new_clone/pass/` — a commit with no new clone → 0
- [ ] `fixtures/diff_new_clone/fail_existing_file/` — a function copied into an existing file
- [ ] `fixtures/diff_new_clone/fail_new_file/` — a file-copy added
- [ ] `fixtures/diff_new_clone/pass_preexisting/` — a clone existed before the commit, the commit touched the copy → 0 (parent-guard)
