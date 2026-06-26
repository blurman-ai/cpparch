# [DRIFT/METHODOLOGY] DRIFT runs on third-party repos require a clean checkout, not a partial one

**Created:** 2026-05-29
**Started:** — (staged for Haiku 2026-06-11, awaiting go)
**Status:** wip (staged)
**Assignee:** Haiku (script + methodology only; PR re-runs are separate research, NOT Haiku)
**Module:** RESEARCH/FIXTURES (methodology #033)
**Priority:** major (skews dogfooding results on third-party repos)
**Difficulty:** S (documentation + helper script, ≤ 1 h)
**Target release:** v0.1
**Blocks:** the reliability of conclusions from any drift run on third-party repositories
**Related:** #033 (ai_drift_dataset), #047 (BOM)

## Symptom

When running DRIFT.1 over a series of PRs from one repo (sequentially in a single
working tree), false positives appear.

Concrete case: **EtherAura/Kartend PR #26** (refactor errorutils, 4 files,
+13/-5 lines, none of them — `settingsdialogform.cpp`):
- First a run with `git clean -fd src + git checkout $parent -- src + ... + git checkout $sha -- src`:
  **26 DRIFT.1** in `ui/dialogs/settings/core/settingsdialogform.cpp`, all pointing
  to files that were NOT changed in the PR.
- The same PR with `git clean -fdx src + git checkout -f $parent -- src` (and likewise for after):
  **0 DRIFT.1**.

Similarly for OreStudio: a dirty run of 4 PRs gave **7 DRIFT.1**, clean — **0**.

## Cause

`git checkout <sha> -- <path>` updates only the files that exist in
`<sha>`. Files that are present in the working tree (from a previous loop iteration
or from master HEAD) but absent in `<sha>` — **stay in the working tree**.

`git clean -fd` removes only untracked files. Tracked files from other revisions
that are listed in the HEAD index are not removed.

Result: during the scan, `archcheck --save-graph-baseline` sees files that should
not be in the `parent` revision, and saves their edges. Then, on checking out
`after`, these files may disappear (if `after` also doesn't contain them) or
stay (if it does). If the file list or their edges changes — DRIFT
fires falsely.

## Empirical confirmation

| Repo | PR | Dirty | Clean | Delta |
|------|----|-------|-------|-------|
| Kartend | #26 errorutils refactor | 26 DRIFT.1 | 0 | -26 (all FP) |
| Kartend | #27 promote uiconstants | 5 DRIFT.1 | 5 DRIFT.1 | 0 (real) |
| Kartend | #34 covers leaf-struct | 0 | 0 | 0 |
| OreStudio | #547 service-to-service auth | 1 DRIFT.1 | 0 | -1 (FP) |
| OreStudio | #558 sql isolation | 0 | 0 | 0 |
| OreStudio | #588 composite instrument | 2 DRIFT.1 | 0 | -2 (FP) |
| OreStudio | #618 ORE instrument | 4 DRIFT.1 | 0 | -4 (FP) |
| IrredenEngine | #727 render LOD | 2 DRIFT.1 | 2 DRIFT.1 | 0 (real) |
| vmecpp | #360, #340 | 0 | 0 | 0 |

All false positives disappear when using `git clean -fdx` + `git checkout -f $ref -- path`.

## Fix

### Short-term: helper script for a safe run

Put into `scripts/drift_run.sh`:

```bash
#!/usr/bin/env bash
# Usage: drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> [graph-baseline-path]
set -euo pipefail
repo=$1; sub=$2; before=$3; after=$4
graph=${5:-/tmp/drift_$(basename $repo)_${before:0:7}.graph.json}

cd "$repo"
git -C "$repo" clean -fdx "$sub" >/dev/null
git -C "$repo" checkout -f "$before" -- "$sub"
archcheck --save-graph-baseline "$graph" "$repo/$sub"
git -C "$repo" clean -fdx "$sub" >/dev/null
git -C "$repo" checkout -f "$after" -- "$sub"
archcheck --drift-baseline "$graph" "$repo/$sub"
```

Guarantees a clean state before each revision.

### Long-term: an archcheck mode with built-in git logic

There's already `archcheck --diff <revspec>`. Extend it into a mode that
itself does a clean checkout of two revisions (or uses `git worktree`
for full isolation), without requiring manual work on the working tree.

`git worktree add /tmp/before <sha>` creates a clean WT in a separate folder.
This eliminates the whole class of working-tree-state problems.

## Impact on milestones

All DRIFT runs from the section "Run 14 — DRIFT re-run after the BOM fix"
need revisiting: `git checkout -- src` was used without `-f` and without `-fdx clean`,
except for the first LibreSprite case where a full `git checkout SHA` was done.

**Confirmed by a clean re-run:**
- LibreSprite #581 → 1 DRIFT.1 (full checkout)
- BambuStudio #10794 → 2 DRIFT.1 (full checkout)
- vmecpp #360, #340 → 0 (clean re-run)
- IrredenEngine #727 → 2 DRIFT.1 (clean re-run)
- Kartend #27 → 5 DRIFT.1 (clean re-run)

**Doubtful (need a re-run):**
- spectre 5 PRs (all were 0; need to verify that 0 is not an FN)
- GWToolboxpp 3 PRs
- IrredenEngine #798, #1200, #1207
- moqx 3 PRs
- AetherSDR 3 PRs
- OreStudio: clean re-run already done → all 0
- Kartend #26, #34: clean re-run done → 0, 0

## Done

- ✅ Step 1: bring `scripts/drift_run.sh` to the contract (2026-06-11)
  - Changed the usage line to `drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> <label>`
  - Added a header comment (3 lines) explaining the reason for the clean checkout
  - Changed `set -e` to `set -euo pipefail`
  - The script now takes `<before-sha>` and `<after-sha>` explicitly (does not compute the parent)
  - Sequence: clean → checkout before → archcheck baseline → clean → checkout after → archcheck drift
  - Absolute path to the binary: `~/projects/cpparch/build/debug/src/archcheck`
- ✅ Step 2: methodology added to `docs/research/ai_drift_runlog.md` (2026-06-11)
  - Added the section "Methodology: clean checkout is mandatory"
  - The empirical Dirty/Clean table copied in full
  - Rule: all DRIFT runs on third-party repos — only via `scripts/drift_run.sh`

## In progress

- (empty)

## Next steps

1. Re-run the doubtful PRs with a full clean checkout — **research, outside the Haiku scope**, a separate pass.
2. ✅→⚠️ `scripts/drift_run.sh` **already exists** (verified 2026-06-11), but doesn't match the contract — bring it up to spec (plan below).
3. Document the methodology in `docs/research/ai_drift_runlog.md` (the file exists) — plan below.
4. `--git-worktree` mode for archcheck — **future, outside the scope of this task**.

## Plan for Haiku (2026-06-11)

Before starting, MUST read in full: this task, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2.

### Permitted forks (facts, verified 2026-06-11)

- `scripts/drift_run.sh` exists, but: the usage line lies (`drift_one.sh`), it takes only `<after-sha>` and prints the parent itself, `set -e` instead of `set -euo pipefail`. Bring it to the contract below — **edit the existing file**, do not create a new one.
- `docs/research/ai_drift_runlog.md` exists — **add the methodology as a section**, do not rewrite the file.
- The flags `--save-graph-baseline` / `--drift-baseline` exist (`src/main.cpp:53-54`) — the script's contract on them is correct.
- The C++ code in this task is NOT touched at all.

### Step 1 — bring `scripts/drift_run.sh` up to spec

Contract: `drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> <label>`. Mandatory:
- `set -euo pipefail`;
- a header comment (3–4 lines): why a clean checkout — `git checkout <sha> -- <path>` does NOT remove files absent in `<sha>`, and `git clean -fd` doesn't touch tracked files; without `-fdx` + `checkout -f`, DRIFT.1 produces FP (see backlog #048);
- keep the sequence for each of the two revisions as in the current script: `git clean -fdx "$sub"` → `git checkout -f "$ref" -- "$sub"` → archcheck;
- the binary — the absolute path `~/projects/cpparch/build/debug/src/archcheck` (as now);
- print `=== $label ===` + DRIFT lines + the `violation(s)` line (as now).

### Step 2 — methodology in `docs/research/ai_drift_runlog.md`

Add the section `## Methodology: clean checkout is mandatory (2026-06-11)`:
- the cause (the wording from the "Cause" section of this task, 1 paragraph);
- the empirical Dirty/Clean table from the "Empirical confirmation" section (copy as is);
- the rule: any DRIFT run on a third-party repo — only via `scripts/drift_run.sh`.

### Control cases / DoD

| Check | Expectation |
|-------|-------------|
| `bash -n scripts/drift_run.sh` | exit 0 |
| self-test (see below) | exit 0, the output contains `=== selftest ===` and a `violation(s)` or `(no violations)` line |
| `grep -c "drift_one" scripts/drift_run.sh` | 0 |

Self-test — **ONLY on a throwaway clone**:
```bash
git clone --quiet ~/projects/cpparch /tmp/drift_selftest
scripts/drift_run.sh /tmp/drift_selftest src be56245 cb6e09d selftest
rm -rf /tmp/drift_selftest
```

**FORBIDDEN to run the script on `~/projects/cpparch` directly**: `git clean -fdx` will wipe uncommitted files, `checkout -f` will knock `src/` back to an old revision. Only a clone in `/tmp`.

### Do not do

- DO NOT re-run the "doubtful PRs" from the "Impact on milestones" section — that's a research step, a separate pass.
- DO NOT touch C++ code, CMake, tests.
- DO NOT implement the `--git-worktree` mode.
- DO NOT commit without an explicit command.

### Escalation (when to stop and hand off to a senior model)

Stop, write here "Blocked: <what/why/what you tried>" and report if: the self-test fails twice for an unclear reason; the script's contract conflicts with something not present in this task; you need a file outside `scripts/drift_run.sh` + `docs/research/ai_drift_runlog.md`. Next — Sonnet, then Opus.

## Result
**Status:** completed — both steps done (the `scripts/drift_run.sh` contract
with the clean-checkout sequence + the methodology in `docs/research/ai_drift_runlog.md`).
**Changed files:** `scripts/drift_run.sh`, `docs/research/ai_drift_runlog.md` (commit 82f91ef)
**Completed:** 2026-06-11
