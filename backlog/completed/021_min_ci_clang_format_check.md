# [CI][DEVX] CI step `clang-format --dry-run --Werror` to protect style

**Created:** 2026-05-27
**Started:** 2026-05-27
**Completed:** 2026-05-27
**Status:** completed
**Module:** CI, DEVX
**Priority:** minor
**Complexity:** XS (1-2 hours)
**Blocks:** —
**Blocked by:** —
**Related:** #019 (cpp_style_realign — the plan's final step mentioned this future-task), #002 (github_actions_ci)

## Goal

Add a `clang-format --dry-run --Werror` step to the GitHub Actions workflow
over all of `src/`, `include/`, `tests/`. Without a CI check, `.clang-format` stays
a recommendation — and drifts over time. With the dry-run check, a PR with
a style mismatch won't merge.

## Context

In #019 we moved to LLVM-base + Allman + `IndentWidth: 2` + `ColumnLimit: 120`.
All existing code was reformatted (commit `7be32d1`). So the guide doesn't
live in a vacuum, a CI gate is needed. This was explicitly flagged in the #019 plan as a
future-task.

The maintainer had `clang-format-11` locally (Astra default via the unversioned
`clang-format`). The Astra repos have `clang-format-18` and `clang-format-19`
available. The CI `build` matrix already has `clang-18` + `clang-tidy-18`. Decided to pin
**clang-format-18**: matches the CI clang-toolchain, native in ubuntu-24.04
(no need to add the LLVM apt repo), and available in Astra too.

## Execution plan

- [x] Install `clang-format-18` locally (apt-get from the Astra repos, version `18.1.8 (9.astra6)`)
- [x] Run `clang-format-18 --dry-run --Werror` — 12 violations in 4 files (drift clang-format 11→18 + longer camelCase after #019 step 3/3)
- [x] Apply `clang-format-18 -i` to all of `src/`, `include/`, `tests/` — separate reformat commit `b5f9a1b` (no semantic changes); SHA in `.git-blame-ignore-revs`
- [x] Add a `format-check` job to `.github/workflows/ci.yml` — parallel to build / static-analysis, runs-on:ubuntu-24.04
- [x] Update `docs/code_style.md` — `clang-format-18` version pinned explicitly + "how to fix locally" instructions

## Key decisions

| Decision | Reason |
|---------|---------|
| `clang-format-18` (not 19, not 11, not unversioned) | Matches CI clang-toolchain (`clang-18` + `clang-tidy-18` in the build matrix); ubuntu-24.04 native (no LLVM apt repo needed); also available in the Astra repos for local use |
| Separate **job** `format-check`, not a step in build | Fails fast (~30 sec), doesn't depend on FetchContent/cache. Parallel feedback: a PR fails on format without waiting for cppcheck / lizard / clang-tidy |
| `--Werror`, not warning-only | Style is either enforced or it isn't. Warning mode means "drift allowed" |
| `find ... \| xargs`, not a shell glob | Robust to a large number of files and spaces in paths |
| Reformat as a separate commit before the CI step | Without it, CI would go red the moment the change merges; a reformat commit is cheaper than a split commit with CI and format fixes |

## Changed files

| File | Change |
|------|-----------|
| `.github/workflows/ci.yml` | + job `format-check` (clang-format-18 --dry-run --Werror) |
| `docs/code_style.md` | "Tools" section: clang-format-18 pinned explicitly + how-to-fix |
| `.git-blame-ignore-revs` | + SHA of the reformat commit `b5f9a1b` |
| `src/`, `include/`, `tests/` (5 files) | reformat commit `b5f9a1b` (alignment of continuation parameters + sort using-declarations) |
