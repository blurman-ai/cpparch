# [CLI][GRAPH][PERF] `--diff`: skip all work if no C/C++ file changed

**Created:** 2026-05-27
**Started:** 2026-05-27
**Status:** completed
**Module:** CLI, GRAPH, PERF
**Priority:** major
**Difficulty:** S (1-2 hours)
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis)

## Goal

Before `git worktree add` and building two graphs, ask git: "did any
`.h/.hpp/.c/.cpp/.cxx/.cc` change at all between baseline and current?" If not —
return 0 and an empty report right away. The architecture could not have changed.

## Context

Measurement on a real `gm` (Release build, 2089 nodes, two related revisions
with no C++ edits):

```
$ time archcheck --diff c34a3bbb..c5dc4121 .     ← 74 seconds
$ time archcheck --graph .                       ←  1 second
$ time git worktree add --detach <tmp> <ref>     ← 34 seconds (one tree!)
```

Distribution: ~68 s goes to two `git worktree add` (full checkout +
hooks), ~1 s — both graph-building passes, the rest — `worktree remove` +
diff logic. **The graph is fast — the bottleneck is git operations**, and in the
"C++ didn't change" case all 74 s are wasted.

This is a typical CI case: a PR with a merge commit from main, where edits to
docs/yaml/python arrived in main — but C++ was not touched. Paying a minute of worktree+scan
to confirm that 0 → 0 is wasteful.

The pre-check `git diff --name-only baseline..current -- '*.h' '*.hpp' '*.c' '*.cpp' '*.cxx' '*.cc'`
takes milliseconds. If the output is empty — exit with exit 0 without worktree-add.

Bonus: if 3 of 2089 files changed — in the future we can build the sub-graph of
**only** those, without scanning the whole project. That is already a separate story
(#TODO: incremental graph build); here it's the minimal fast-path.

## Execution plan

- [x] Add `git_state::changedCppFiles(repoRoot, baselineRef, currentRef) -> vector<path>` via `git diff --name-only --diff-filter=ACMRD <a> <b>` (extension filtering is done in C++ via the reusable `scan::hasProjectExtension`)
- [x] In `run_diff` (`src/main.cpp`) after `parseRevspec` and `findRepoRoot` — fast-path: if the list is empty, print "no C/C++ files changed; skipping graph build" and return 0
- [x] Edge-case `WORKTREE`: `git diff --name-only <baseline>` (without a second ref) for tracked-modified + `git ls-files --others --exclude-standard` for untracked
- [x] Extensions: extracted the public `archcheck::scan::hasProjectExtension()` — single source of truth (12 extensions from `kExtensions`)
- [x] Tests: 3 new integration cases in `tests/integration/diff/git_diff_test.cpp` — mixed-change, docs-only-empty, WORKTREE-modified-and-untracked
- [x] Re-run on `gm`: docs-only commit — **6 ms** (was 74 s, 12000× speedup). 18 of the last 30 commits in gm are fast-path candidates

## Done

- **CLI fast-path** in [src/main.cpp:130-141](src/main.cpp#L130-L141) (`tryFastPathNoCppChanges`) — before all git worktree operations. Prints `no C/C++ files changed; skipping graph build`, exit 0.
- **`changedCppFiles`** in [include/archcheck/git/git_state.h](include/archcheck/git/git_state.h) + [src/git/git_state.cpp](src/git/git_state.cpp) — two internal helper functions (`collectChangedVsWorktree`, `collectChangedTwoRefs`), filtering via `scan::hasProjectExtension`.
- **`hasProjectExtension` exposed publicly** in [include/archcheck/scan/project_files.h](include/archcheck/scan/project_files.h) — without duplicating the extension list.
- **Tests**: 134 cases (was 131), +3 new on `changedCppFiles`. All 407 assertions pass.
- **Measurement on gm Release**: docs-only commit pair (`d180d152..469d7d02`) — **0.006 s** instead of 74 s. Commit pair with one `.cpp` (`c34a3bbb..c5dc4121`) — the path did not fire (correctly), full run 68 s.
- **gm analytics**: 18/30 of the last commits have `changedCppFiles = 0` → benefit from the fast-path.
- **Lizard 0 warnings, clang-format-18 clean**.

## In progress

- (empty) — task completed.

## Next steps

1. **Ready for `/commit`**.
2. **Next**: task [#024](backlog/new/024_maj_in_memory_fs_for_diff.md) — in-memory FS removes the remaining 68 s (when C++ did change, but only a little).

## Key decisions

| Decision | Reason |
|---------|---------|
| Pre-check via `git diff --name-only`, not via comparing file lists from `discoverFiles` | git knows the delta in O(commits), not O(files); works even without a checkout |
| Patterns hard-coded (`*.h *.hpp *.c *.cpp *.cxx *.cc`) | YAGNI: an extension config will arrive with the general config loader |
| Skip only when exactly zero changes | If even one C++ file changed — go down the full path; incremental graph is a separate task |
| Print an explicit "skipped" message to stdout | CI logs should explain why it was so fast |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/git/git_state.h` | + `changedCppFiles(repoRoot, baselineRef, currentRef)` |
| `src/git/git_state.cpp` | + implementation (one `runGit` + split lines) |
| `src/main.cpp` | + guard in `run_diff` |
| `tests/integration/diff/git_diff_test.cpp` | + case "PR touches only docs → no worktree" |
