# [PERF][SCAN] per-commit new-clone scan runs the WHOLE snapshot → timeout on large repos

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** new
**Module:** SCAN / DUPLICATION / DIFF
**Priority:** major
**Related:** #123 (new-clone drift), #147 (lex-cap memory), #145 (trending run, where it was caught)

## Symptom (trending run #145)

28 of 56 trending repos — **timeout-blacklist** (`--timeout 120`/commit, guard blacklists after 2):
tensorflow ok=2, opencv ok=1, envoy ok=2, godot ok=1, mame ok=0, onnxruntime/openvino ok=1-2,
FreeCAD, Cataclysm-DDA… For the largest (most interesting) repos there's almost no data.

## Cause

For EVERY commit, `detectNewClones` (`new_clone_drift.cpp`) runs TWO whole-tree
duplication scans: `parentPairKeys(parentSnapshot)` = `scanForDuplication` over the WHOLE parent,
plus `scanForDuplication` over the WHOLE child. Clone detection is ~O(files × tokens) over the entire tree →
on a repo with thousands of files a single commit doesn't fit in 120s. #147 fixed memory, but not wall-clock.

## Fix direction

We need a diff scope: search for clones only in the FRAGMENTS that intersect the commit's changed lines, not
in the whole tree. Right now the `touchesAdded` filter is applied AFTER the full scan — too late. Options:
- index only changed files against a tree index (rather than tree×tree);
- or a separate budget/skip duplication on huge snapshots (like complexity bulk_skip #117).

## What to check
- [ ] Profile: where time goes on a tensorflow commit (parentPairKeys vs new scan).
- [ ] Scope clone search to changed files; verify that new-clone findings aren't lost (#123 control set 10/10).
- [ ] Corpus re-measure: how many repos were previously blacklisted and now pass.
- [ ] (related to #145) after the fix — re-measure the large trending repos.
