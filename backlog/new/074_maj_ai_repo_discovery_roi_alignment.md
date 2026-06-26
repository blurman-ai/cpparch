# [RESEARCH][DISCOVERY] Consolidate discovery tech debt across ROI filters, giant-skip, and the resumable pipeline

**Created:** 2026-06-02
**Started:** —
**Status:** new
**Module:** RESEARCH / SCAN / DISCOVERY
**Priority:** major
**Difficulty:** M
**Blocks:** fast top-up of an AI-dense corpus without wasted network/disk/time
**Blocked by:** —
**Related:** #054 (ai_repo_duplication_run), #066 (airepo_remeasure_clonefail), #067 (overnight_eye_verification), #073 (tech_debt_alignment_cleanup)
**Source of truth:** `experiments/ai_repo_run/measure_candidates.sh`, `experiments/ai_repo_run/remeasure_resumable.sh`, `experiments/ai_repo_run/clone_expand.sh`, `experiments/ai_repo_run/resume_all.sh`

## Goal

Remove the wasted time on giant/established repositories and reduce the discovery/resume pipeline
for the AI-repo corpus to a single real contract.

The task is not about new metrics, but about **ROI and the honesty of the pipeline**:

- don't download knowingly useless giants;
- pull only the history we actually measure;
- distinguish `skip` from `CLONEFAIL` rather than mixing them;
- remove the mismatch between what `#066` says and what's actually in the code.

## Symptom

Right now discovery has already been partially optimized, but the optimizations are smeared and live in
different branches of the pipeline:

1. `resume`/`remeasure` have a giant name-filter.
2. The main `measure` path has no giant-filter.
3. `clone_expand` has a size-API precheck, but that's already a late stage.
4. `measure_candidates.sh` still does a plain `git clone --filter=blob:none`
   without `--shallow-since=2025-05-01`.
5. On a clone failure `measure_candidates.sh` immediately writes `CLONEFAIL`, without trying
   to distinguish "repo is not a candidate / no fresh history" from a real network failure.

Result:

- we waste time on giant repos of the `chromium`/`llvm`/`webkit` class;
- some `CLONEFAIL`s are noise and carry no useful signal;
- the resume path and the main path behave differently;
- the documentation in `#066` already promises optimizations that aren't in the code.

## Confirmed mismatch

State check as of 2026-06-02:

- `resume_all.sh` does indeed bring up the resumable pipeline after a reboot.
- the giant name-filter actually exists only in `remeasure_resumable.sh`.
- `measure_candidates.sh` does **not** use `--shallow-since=2025-05-01`.
- `measure_candidates.sh` does **not** do an API fallback to separate `skip` from `CLONEFAIL`.
- `clone_expand.sh` uses the API only for a size precheck, not for the measure phase.
- meanwhile `backlog/wip/066_...` already describes `shallow-since` and the API fallback as implemented.

This is no longer just a perf TODO, but a **research-methodology debt + backlog/code mismatch**.

## Why it matters

For the current phase we don't need a "representative slice of all large C++ repos".
We need a corpus with the maximum probability of useful signal per unit of time.

Giant established repos give bad ROI:

- expensive to download and retry;
- often have low AI concentration;
- likely pass through a strong review/process barrier;
- give fewer chances of new drift signal than mid-size/high-velocity AI-dense repos.

So the pipeline should be tuned for:

- **high velocity**;
- **AI density**;
- **moderate size**;
- **early giant-skip**;
- **minimal false CLONEFAIL**.

## What to do

### 1. Thread `--shallow-since=2025-05-01` into the real measure path

- [ ] Add a shallow-fetch/clone, limited to the post-May window, to `measure_candidates.sh`.
- [ ] Verify that measuring `git log --since=2025-05-01` stays correct on shallow history.
- [ ] Explicitly document the fallback if a specific forge/repo doesn't support the needed shallow path.

### 2. Raise the giant denylist into the main funnel

- [ ] Move the GIANT regex into one place.
- [ ] Apply giant-skip not only in `remeasure_resumable.sh`, but also in the main `measure/discovery` path.
- [ ] Record the status of such repos as `TOOBIG-skip`, not as `CLONEFAIL`.

### 3. Introduce an API fallback to distinguish `skip` vs `CLONEFAIL`

- [ ] On a shallow/clone failure do one cheap API check.
- [ ] If the repo has no needed fresh history / is not a candidate — write `skip`, not `CLONEFAIL`.
- [ ] Leave only real network/availability failures as `CLONEFAIL`.

### 4. Reduce the main and resumable paths to one contract

- [ ] Remove the situation where the resume path is smarter than the main one.
- [ ] Check `discover_finish*.sh`, `measure_candidates.sh`, `remeasure_resumable.sh`, `finish_resumable.sh` for unified selection rules.
- [ ] Make reboot/resume not change the selection semantics, only continue the same work.

### 5. Bring the backlog/doc state in line with reality

- [ ] Update `#066`: separate "implemented in code" from "intended / to be implemented".
- [ ] If some optimizations remain only in the resumable branch, that must be written explicitly.
- [ ] Remove the false contract from the task log, so the next run doesn't proceed from wrong premises.

## Acceptance criteria

- [ ] `measure_candidates.sh` no longer pulls the full history when the post-May window is enough.
- [ ] giant repos are cut off before the heavy clone in both the normal and the resumable mode.
- [ ] `CLONEFAIL` after a run is noticeably cleaner: only real failures remain, not "not a candidate".
- [ ] Code and backlog describe identically which optimizations actually work.
- [ ] There is one clear discovery pipeline, not two similar but logically unequal ones.

## Don't do in this task

- don't change the drift metrics themselves;
- don't introduce an ML/classifier for AI repos;
- don't expand the product scope of `archcheck`;
- don't mix this with verification of copy-paste precision.

## Next steps

1. First fix the alignment between `#066` and the code.
2. Then thread `shallow-since` and giant-skip into the main `measure` pipeline.
3. After that, recompute discovery ROI and update the methodology report.
