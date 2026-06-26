# [TESTS][GRAPH] Cover `--diff` with a PR-with-merge-commit scenario

**Created:** 2026-05-27
**Started:** 2026-05-28
**Status:** completed
**Module:** TESTS, GRAPH
**Priority:** minor
**Difficulty:** S (1-2 hours)
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis)

## Goal

Add an integration test in `tests/integration/diff/git_diff_test.cpp` that
checks the behavior of `--diff` on a PR whose HEAD is a merge commit (not a
fast-forward), and confirm that `git worktree add --detach` correctly
materializes both parents without surprises.

## Context

In the plan for task #018 there was an item "PR with a merge commit" — it is not covered. In
practice `git worktree add --detach <ref>` works with a merge commit the
same way as with any ref (gives a snapshot, not a diff), but without a test it is easy
to get a regression when someone starts parsing the parents by hand.

The scenario we want to check:

```
        A (baseline)
       / \
      B   C       (two feature branches add different edges)
       \ /
        M (merge)  ← HEAD
```

`--diff A..M` should see the **union** of changes from B and C (edges from
both branches), not just the diff of M against one of the parents.

## Execution plan

- [x] Extend `tests/integration/diff/git_diff_test.cpp` with a new TEST_CASE "merge-commit"
- [x] Build `A → B (adds a->c) → checkout A → C (adds a->d) → merge B` (via `git tag A`/`git tag C`)
- [x] Assert: `diffRefs("A", "HEAD")` shows both edges `a->c` and `a->d` in addedEdges
- [x] Assert: `diffRefs("C", "HEAD")` shows only `a->c` (what the merge brought from feat-b)
- [x] lizard and clang-format clean

## Done

- Helper `buildMergeRepo()` builds A→B (a->c) and A→C (a->d), merges feat-b into feat-c
  with manual conflict resolution on a.h (union of both includes). HEAD is a real
  merge commit, checked by `git rev-parse HEAD^2`.
- TEST_CASE `merge-commit HEAD → A..M sees union of edges from both parents`
  `[diff][git][integration][merge]`: 26 assertions, green.
- A SHA via `git rev-parse` was not needed — `git tag A` / `git tag C` on the needed
  commits solve it without extending `runIn`.
- Confirmed: `git worktree add --detach` (inside `materializeRef`) correctly
  materializes the merge snapshot, not one of the parents.

## Next steps

1. `/commit` — `test(diff): cover merge-commit PR scenario in --diff (#022)`
2. Close the task (move to `backlog/completed/`).

## Key decisions

| Decision | Reason |
|---------|---------|
| Test only, no production-code edits | Hypothesis: `git worktree add` already handles a merge commit correctly. If the test is green — the behavior is pinned by a regression; if red — it becomes clear what to fix |

## Changed files

| File | Change |
|------|-----------|
| `tests/integration/diff/git_diff_test.cpp` | + 1-2 TEST_CASEs; possibly a helper `commitAllAndTag` or `captureSha` |
