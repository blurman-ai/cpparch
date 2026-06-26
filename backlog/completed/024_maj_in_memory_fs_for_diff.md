# [SCAN][GRAPH][PERF] In-memory FS: read git blobs directly, without worktree-add

**Date created:** 2026-05-27
**Date started:** 2026-05-27
**Date completed:** 2026-05-27
**Status:** completed
**Module:** SCAN, GRAPH, PERF
**Priority:** major
**Difficulty:** L (3-5 days)
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #023 (skip_when_no_cpp_changes)

## Goal

Teach `scan/` and `graph/graph_builder` to accept a **file system
abstraction** (a source of "list of files + content reader"), and for `--diff`
use an implementation on top of `git cat-file --batch` / libgit2 that reads
blobs directly from the object DB without writing to disk.

The goal is to remove the main cost from `--diff`: 32-35 s × 2 worktree-add per
run. Rough estimate: <2 s for both passes instead of ~70 s.

## Context

[scripts/bench_materialize.sh](../../scripts/bench_materialize.sh) compared
5 ways to materialize a ref into a directory on `gm` (2089 C++ files, Release):

| Method | Materialize | Scan |
|---|---|---|
| `git worktree add` | 34.8 s | 0.18 s |
| `git archive | tar -x` | 32.5 s | 0.15 s |
| `git checkout-index` | 36.1 s | 0.17 s |
| `git restore --source` | 33.2 s | 0.17 s |
| `git clone --shared` | 47.1 s | 0.44 s |

**All disk-based methods are I/O-bound** (writing 2089 files to /tmp). The
scanner itself is ~170 ms. Swapping `worktree add` for `archive` gives 7% — not worth
the candle on its own.

The existing tools for many-commit analysis (PyDriller, CodeMaat)
**do not check out** — they read blobs directly from the object DB via
pygit2/libgit2 or `git cat-file --batch`. This is the right path.

## Execution plan

- [x] Design `archcheck::scan::FileSource` (no `I` prefix — the style guide forbids it): `list() -> vector<ProjectFile>`, `read(path) -> string`
- [x] Move `scan::discoverFiles` + `buildGraphForPath` to work through this abstraction (the current code becomes `DiskFileSource`)
- [x] Add `git::GitObjectFileSource(repo, ref)`: enumerate via `git ls-tree -r <ref>`, content via `git cat-file --batch` (one long-lived process, batch protocol)
- [x] In `run_diff` use `GitObjectFileSource` for refs, `DiskFileSource` for `WORKTREE`
- [x] Benchmark: on `gm`, `--diff` dropped from ~60-90 s (disk methods) to **0.49 s** (in-memory). The "<5 s" goal was overshot with room to spare, ~120× speedup.
- [x] Tests:
  - 4 unit tests for `GitObjectFileSource` on a temp git repo (extension filter, blob byte-equality, parity vs DiskFileSource, missing path → empty)
  - 4 integration `--diff` tests through the memory path — added edge / closed cycle / no-op / `<ref>..WORKTREE`. Results identical to the disk variant.
- [x] CLI flag `--diff-mode=disk|memory`, default `memory`. `--help` updated.
- [x] `materializeRef` kept (escape hatch via `--diff-mode=disk`). Decided not to remove: useful for the case of odd repos (submodules / LFS / sparse). After v0.2, if there are no complaints — it can be removed.
- [x] `scripts/bench_materialize.sh` updated with a 6th method `git-objects` (materialize=0 ms, scan = full `--diff` cycle via `git cat-file --batch`).

## Done

- **2026-05-27** — `scan::FileSource` (abstract base) + `scan::DiskFileSource` (wrapper over the current `discoverFiles` + `ifstream`). Named without an `I` prefix per [docs/code_style.md](../../docs/code_style.md) §"What we do NOT do".
- **2026-05-27** — `graph::buildGraphForSource(FileSource&)` became the source-of-truth pipeline, `buildGraphForPath(root)` — a thin wrapper via `DiskFileSource(root)`. Behavior byte-for-byte unchanged.
- **2026-05-27** — `git::GitObjectFileSource(repoRoot, ref)`: one long-lived `git cat-file --batch` process (fork/exec, full-duplex pipes, RAII close+wait). `list()` via a one-shot `git ls-tree -r --name-only <ref>` + filter by `hasProjectExtension` and excluded-dir-segments. `read()` parses the batch protocol (`<sha> <type> <size>\n<payload>\n`); blob → payload, otherwise → empty.
- **2026-05-27** — `main.cpp::run_diff` takes a `DiffMode`. For refs — `GitObjectFileSource`, for `WORKTREE` — `DiskFileSource`. CLI: `--diff-mode=disk|memory`, default `memory`. Disk mode kept as an escape hatch.
- **Benchmark on gm (2089 files, Release):**

  | Method | Materialize | Scan |
  |---|---|---|
  | worktree | 28.3 s | 0.16 s |
  | archive | 32.3 s | 0.17 s |
  | checkout-index | 27.9 s | 0.19 s |
  | restore | 29.4 s | 0.17 s |
  | shared-clone | 46.5 s | 0.47 s |
  | **git-objects** | **3 ms** | **485 ms (full --diff = 2 graphs)** |

  Full `--diff` cycle through the memory path = **0.49 s vs ~60-90 s** for the disk methods (≈ 2× materialize+scan). **~120× speedup.**

- All **142 tests green** (131 → 142; +4 unit GitObjectFileSource, +4 integration memory-parity, +3 changed-file tests from the neighboring step). Lizard `--CCN 15 --length 30 --arguments 5 --warnings_only` — clean.

## Principle of operation

```
run_diff(revspec, root, mode):
   parseRevspec → (baseline, current)
   findRepoRoot(root) → repoRoot
   tryFastPathNoCppChanges    # from #023
   for side in (baseline, current):
      if side == WORKTREE: DiskFileSource(repoRoot)
      elif mode == Memory:  GitObjectFileSource(repoRoot, side)
      else:                 materializeRef + DiskFileSource(tree)
      buildGraphForSource(source) → GraphBuildResult
   buildRegressionReport(baseline, current) → report
```

`GitObjectFileSource` owns one long-lived `git cat-file --batch` process (RAII). On `read(path)` it writes `<ref>:<path>\n` to stdin, reads the header + payload from stdout. `list()` — a one-shot `git ls-tree -r --name-only <ref>` (a simpler protocol; the cost is one extra fork per run, ~3 ms).

`FileSource` — an abstract base with `list()` + `read(path)`. `buildGraphForSource(FileSource&)` — the single point of truth; `buildGraphForPath(root)` — a wrapper via `DiskFileSource(root)`.

## Key decisions

| Decision | Reason |
|---------|---------|
| In-memory path via `git cat-file --batch`, not libgit2 | We don't pull in a new dependency (libgit2). `cat-file --batch` — one process, batch protocol, minimal overhead compared to per-file `git show`. libgit2 — an optimization for the next step, if needed |
| `IFileSource` abstraction, not two different pipelines | DRY: tests, sanity checks, graph logic — one set. The benchmark already showed scan costs 0.17 s, switching to the abstraction shouldn't worsen that much |
| `--diff-mode=disk` as an escape hatch | For the case of odd repos (submodules? LFS? sparse?) the user should have a way to fall back to the proven worktree method. The flag can be removed after v0.2 if there are no complaints |
| `WORKTREE` (the current working tree) stays via DiskFileSource | The working tree is precisely files on disk (which may be uncommitted), there are by definition no blobs there. Don't try to unify it |
| Not going to libgit2 at this step | YAGNI. First measure the `cat-file --batch` gain; if < 10× — libgit2 isn't justified. If ≥ 10× — discuss libgit2 as a next step |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/scan/file_source.h` | new — concept/interface `IFileSource` |
| `include/archcheck/scan/disk_file_source.h` | new — wrapper over `std::filesystem` + `std::ifstream` |
| `src/scan/disk_file_source.cpp` | new — implementation (logic moved from `discoverFiles` + readFile) |
| `include/archcheck/git/git_object_file_source.h` | new — `GitObjectFileSource(repo, ref)` |
| `src/git/git_object_file_source.cpp` | new — implementation via `git cat-file --batch` |
| `include/archcheck/graph/graph_builder.h` | + overload `buildGraphForSource(IFileSource&)` |
| `src/graph/graph_builder.cpp` | + implementation via the source-abstract pipeline |
| `src/main.cpp` | run_diff: use `GitObjectFileSource` for git refs; add `--diff-mode` |
| `tests/unit/git/git_object_file_source_test.cpp` | new — unit |
| `tests/integration/diff/git_diff_test.cpp` | + check that the in-memory path gives the same results as the disk path (parametrize) |
| `scripts/bench_materialize.sh` | + 6th method `git-objects` (in-memory, no disk writes) |
