# [SCAN] #056 --diff: rename-detection (FP-other fix)

**Created:** 2026-05-31
**Status:** wip
**Module:** SCAN
**Priority:** major
**Related:** #060 (validation), #056

## Goal
Remove FP-other (~17% in iter1): `--diff` reports a renamed file as
ADDED-new-path ⟵ BASE-old-path (the same file moved, not a duplicate).

## Fix
`changedSourceFiles`: `git diff --numstat` → `git diff -M --numstat`. A rename
is shown as `old => new` and is filtered out by existing code → the file is not
processed, no phantom pair is created.

## Done
- [x] `-M` added (main.cpp ~839), #056 rebuilt at 22:10.

## Verification
- [ ] iter2: pairs with a rename pattern (same basename, different dir) disappeared/dropped.

## Outcome
**Status:** completed — `git diff -M --numstat` added (main.cpp), phantom
rename pairs are no longer created. Verification was done as part of #060 iter2 (commit 0beb2f7)
with a clarification: most same-basename pairs turned out to be real copies, not
git-renames (i.e. the fix is correct, and the FP-other class was smaller than the iter1 estimate).
**Completed:** 2026-06-11 (actually earlier — closed during a backlog pass)
