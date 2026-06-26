# [CLI][GRAPH] Git-based analysis: compute the graph delta, not a full snapshot

**Created:** 2026-05-26
**Started:** 2026-05-27
**Status:** completed
**Module:** CLI, GRAPH
**Priority:** critical
**Difficulty:** L (3-5 days)
**Blocks:** —
**Blocked by:** — (#015, #016 closed)
**Related:** #008 (dependency_graph_foundation), #009 (ai_drift_regression_rules, future)

## Goal

Make archcheck in CI say not "the project has 27 cycles" (useless
for reviewing a PR on a legacy project), but **"this PR introduced 1 new cycle and 3 new
unresolved imports"**. That is, compute the architectural **delta** against a
git ref (commit, branch, range), not against a full snapshot.

## Context

Right now `archcheck --graph <path>` builds and analyzes the **entire** project
tree. On the real `gm` (2192 files) it finds 2 cycles in the Unigine SDK,
708 unresolved, 138 ambiguous. In a PR context this is noise: there are no new violations,
the old ones are historical. CI should focus on **the changes
this particular PR introduced**.

There are two comparison paths, both valid:

1. **Baseline file (#016)** — `.archcheck/graph-baseline.yaml` in the repo.
   PR validation: build the graph of the current state, compare it with the baseline,
   report only the delta. Downside: you need to "attach" the baseline to main and
   update it on deliberate architectural changes.

2. **Git-based (this task)** — `archcheck --diff main..HEAD` or
   `archcheck --since HEAD~1`. The tool itself does `git checkout` (or
   `git show:file`) for the two states, builds two graphs, computes the delta.
   Downside: more expensive (two passes), needs git integration.

Often these approaches **complement** each other:
- baseline — for long periods and large one-off refactorings;
- git-diff — for every PR in "is this getting worse here" mode.

The task is to implement the git-diff path. The baseline path goes into #016 in parallel
and reuses the same primitives from #015.

## Execution plan

- [x] Define the CLI form: the canonical `--diff <revspec>` (`a..b` or a single `<ref>` = `<ref>..WORKTREE`). `--since`/`--vs-base` decided NOT to introduce — a git revspec covers both scenarios
- [x] Decide how to read the "past" state: `git worktree add --detach --quiet <tmp> <ref>` (fork/exec). For the current worktree — the sentinel `WORKTREE` with no checkout. Without libgit2; libgit2 — a future optimization on demand
- [x] Implement `include/archcheck/git/git_state.h` / `src/git/git_state.cpp`: RAII `Worktree`, `materializeRef`, `findRepoRoot`, `parseRevspec`. No shell injection (fork/exec + execvp with argv)
- [x] Build two graphs (baseline, current), reusing the pipeline from `--graph` through the new helper `archcheck::graph::buildGraphForPath()`
- [x] Apply the diff primitives from #015 (`addedEdges`, `removedEdges`, `grownSccs`) inside `archcheck::diff::buildRegressionReport`
- [x] Reporter: `archcheck::diff::writeTextReport` — "added_edges/removed_edges/grown_cycles", followed by a listing with file:line (no line yet, line will come when location metadata appears in EdgeRef)
- [x] Exit codes: 0 = `!hasRegression`, 1 = `hasRegression` (new edges OR grown cycles; removed-only ≠ regression), 2 = invalid revspec / not-a-repo / git failure
- [x] Tests: 6 integration tests via `mkdtemp` + git CLI: an added edge, closing a cycle, a no-op (docs), single-ref→WORKTREE, findRepoRoot from a subdirectory, an invalid ref
- [x] CI example: `.github/workflows/example_archcheck_pr.yml` — `archcheck --diff origin/main..HEAD .` + upload-artifact

## Done

- **CLI**: added `--diff <revspec> [path]` to [src/main.cpp](src/main.cpp) (path defaults to cwd).
- **Revspec parser**: `parseRevspec` ([include/archcheck/git/git_state.h:17](include/archcheck/git/git_state.h#L17)); `a..b`, `<ref>`, rejects `...`/empty sides.
- **Git materialization**: RAII `Worktree` (move-only) + `materializeRef` via fork/exec `git worktree add --detach`. Cleanup via `git worktree remove --force` + `fs::remove_all` (best-effort). Sentinel `WORKTREE` for the current working tree without a checkout.
- **Graph build pipeline extracted**: `buildGraphForPath()` ([include/archcheck/graph/graph_builder.h](include/archcheck/graph/graph_builder.h)). `--graph` and both `--diff` passes now call one helper.
- **Regression report**: `RegressionReport` ([include/archcheck/diff/regression_report.h](include/archcheck/diff/regression_report.h)) — a DTO + `buildRegressionReport` (resolves NodeId to strings) + `writeTextReport`.
- **Tests**: 20 new `[diff]` cases (parseRevspec, regression_report, end-to-end via a temp git repo). 131 cases total, 380 assertions, 100% pass. Lizard 0 warnings (CCN ≤15, length ≤30). clang-format-18 clean.
- **CI example**: [.github/workflows/example_archcheck_pr.yml](.github/workflows/example_archcheck_pr.yml).
- **Smoke on our own repo**: `archcheck --diff HEAD .` correctly shows 19 new edges and 1 removed from the current PR; exit code 1.

## In progress

- (empty) — the task is ready for commit/review.

## Next steps

1. **Ready for `/commit`** — the task lands entirely in one merge, as the user requested (outside the usual "≤50 lines/commit" mode).
2. **Next (outside this task)**: line numbers in `EdgeRef` (the reporter currently shows only paths; for file:line we need to carry location from the scanner — a separate task).
3. **Optimization (on demand)**: switching from fork/exec to libgit2 — when profiling shows that worktree add dominates.
4. **GitHub PR commenting**: the example workflow uploads an artifact but does not comment on the PR — adding a step via `actions/github-script` can come later, once we settle on the format.

## Key decisions

| Decision | Reason |
|---------|---------|
| Git-diff and baseline — two equal paths | They cover different CI scenarios / different teams; not competing |
| fork/exec git first, then libgit2 if needed | Fewer dependencies, simpler to start; libgit2 is an on-demand optimization |
| Exit code 1 only on a "regression", not on "old violations" | CI should let a PR through if it made nothing worse, even if the legacy is dirty |
| Pass refs as a git revspec (`a..b`, `HEAD~1`) | A familiar format, git parses it itself |
| Canonical CLI: `--diff <revspec>`; a single `<ref>` → `<ref>..WORKTREE` | `--since` and `--vs-base` are syntactically redundant: a revspec covers both. One form — less confusion in the docs |
| `git worktree add --detach <tmp> <ref>` vs `git show :file` | a worktree gives a real tree → the existing `buildGraphForPath` pipeline is reused. `git show` would have to be wrapped in a virtual FS provider — not justified at the start |
| Sentinel `WORKTREE` — the current working directory, without a checkout | For a local run and `--diff <ref>` we skip worktree add (free). The RAII wrapper degenerates into a non-owning one |
| Removed-only diff ≠ regression | A PR that only removes edges makes the architecture stricter — CI should let it through |
| Resolved NodeId → paths in `RegressionReport` upfront | The report outlives the baseline/current graphs; the reporter does not depend on the lifetime of the two DependencyGraphs |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/graph/graph_builder.h` | new — the public helper `buildGraphForPath()` (a shared entry for `--graph` and `--diff`) |
| `src/graph/graph_builder.cpp` | new — implementation (logic moved out of main.cpp) |
| `include/archcheck/git/git_state.h` | new — `Revspec`, `Worktree` (RAII), `parseRevspec`, `findRepoRoot`, `materializeRef` |
| `src/git/git_state.cpp` | new — fork/exec git (`execvp`, no shell). Helpers: `execChild`, `drainFd`, `collectChild` |
| `include/archcheck/diff/regression_report.h` | new — `RegressionReport` (paths-resolved), `buildRegressionReport`, `writeTextReport` |
| `src/diff/regression_report.cpp` | new |
| `src/main.cpp` | + `--diff <revspec> [path]` dispatch; `run_graph` simplified to a buildGraphForPath call; `--diff` added to `print_help` |
| `src/CMakeLists.txt` | + 3 new sources in `archcheck_core` |
| `tests/CMakeLists.txt` | + 3 new test units |
| `tests/unit/git/git_state_test.cpp` | new — 6 parseRevspec cases |
| `tests/unit/diff/regression_report_test.cpp` | new — 4 buildRegressionReport/writeTextReport cases |
| `tests/integration/diff/git_diff_test.cpp` | new — 6 end-to-end cases via `mkdtemp` + git CLI |
| `.github/workflows/example_archcheck_pr.yml` | new — an example CI job for users |
| `backlog/wip/018_crt_git_diff_analysis.md` | move from `new/` + checkpoint |

**NOT touched in this PR**: `docs/architecture-spec.md` — clarifying the two paths (baseline+git-diff, how they coexist) waits until enough user experience accumulates to write a meaningful section. Mark it as a separate task.
