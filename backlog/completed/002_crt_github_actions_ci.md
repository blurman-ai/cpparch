# [BUILD] CI on GitHub Actions

**Created:** 2026-05-26
**Start date:** 2026-05-26
**Completion date:** 2026-05-26
**Status:** done
**Module:** BUILD
**Priority:** critical
**Complexity:** M (a day)
**Blocks:** #001 (dogfood_static_analyzers)
**Blocked by:** ~~#004 (project_skeleton)~~ — unblocked
**Related:** —

## Goal

Every push and PR builds Debug on Linux gcc+clang and runs the tests. A green CI from the first real code commit.

## Execution plan

- [x] `.github/workflows/ci.yml`: matrix Linux (ubuntu-24.04) × {gcc-13, clang-18}, Debug
- [x] Caching `build/debug/_deps/` by the key `hashFiles('CMakeLists.txt', 'tests/CMakeLists.txt', 'src/CMakeLists.txt')` (CMakePresets.json removed in #004, key updated)
- [x] Steps: configure → build → ctest --output-on-failure → smoke binary
- [x] Fail on warnings (the flags are already in CMakeLists via `-Werror`)
- [x] Status badge in README
- [x] Do **not** add macOS/Windows in the first PR — a separate task once Linux is green
- [x] Do **not** add a Release build until a release workflow appears
- [x] `concurrency` block: cancel-in-progress for PR/push on the same ref (saves actions minutes)

## Done
- **2026-05-26** — `.github/workflows/ci.yml` created. Trigger: push to master + PR to master. Matrix: `ubuntu-24.04 × {gcc-13, clang-18}`. Steps: checkout → install (ninja-build, cmake, gcc-13 / clang-18) → cache `build/debug/_deps/` via actions/cache@v4 → cmake configure (Ninja, Debug) → cmake --build → ctest --output-on-failure → binary smoke (`--version`, `--help`, exit 2 on unknown). Concurrency group `ci-${ref}` with cancel-in-progress to save minutes on rapid pushes.
- **2026-05-26** — README got a CI badge (the URL is on `cpparch` for now; after renaming the repo on GitHub there is auto-redirect).

## In progress
- (empty)

## Next steps

1. Minimal yaml on gcc-13 only, confirm it's green
2. Add clang-18 to the matrix
3. Status badge

## Key decisions

| Decision | Reason |
|---------|---------|
| Linux only at the start | Spec: a static binary for Linux/mac/Win, but MVP — Linux. Cross-platform later |
| Debug only | CLAUDE.md: we don't run Release without a request |

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| `.github/workflows/ci.yml` | new — matrix Linux × {gcc-13, clang-18}, Debug | `f9a8628` |
| `.github/workflows/ci.yml` | bump actions/checkout@v4→v5, actions/cache@v4→v5 (Node 24) | `a09d148` |
| `README.md` | CI badge | `f9a8628` |

## How it works

GitHub Actions workflow `.github/workflows/ci.yml`:

1. **Trigger:** `push` to master + `pull_request` to master.
2. **Concurrency:** `ci-${ref}` with `cancel-in-progress` — rapid pushes don't burn extra actions minutes.
3. **Job `build`** on `ubuntu-24.04` in the matrix `gcc-13` × `clang-18` (fail-fast disabled).
4. **Steps:**
   - `actions/checkout@v5` — Node 24.
   - `apt-get install` — ninja-build, cmake, the specific compiler from the matrix.
   - `actions/cache@v5` — cache `build/debug/_deps/` by the key `deps-Linux-{compiler}-hashFiles(CMakeLists.txt + src/CMakeLists.txt + tests/CMakeLists.txt)`. The restore-keys prefix `deps-Linux-{compiler}-` catches unrelated cache hits.
   - `cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug` — configuration. On a cache hit ryml + Catch2 are already in `build/debug/_deps/`.
   - `cmake --build build/debug` — compilation.
   - `ctest --output-on-failure` in `build/debug` — running the Catch2 suite.
   - Binary smoke: `--version`, `--help`, exit 2 on an unknown arg.
5. **Status badge** in README rendered by the URL `actions/workflows/ci.yml/badge.svg`.

## What controls it

- **Action versions:** `actions/checkout@v5`, `actions/cache@v5` — both Node 24.
- **Matrix:** matrix.include in the workflow. To add a compiler — add an entry with `compiler / cc / cxx / apt-pkg`.
- **Cache key:** on a change to `CMakeLists.txt`/`src/CMakeLists.txt`/`tests/CMakeLists.txt` the cache is invalidated. This is the correct behavior: if a dependency version changed — refetch it.
- **Platform:** ubuntu-24.04. Cross-platform (macOS, Windows) — a separate task.

## What it relates to

- **#004 (project_skeleton)** — this CI builds what's in #004. If the CMake scaffold changes, the workflow may need editing.
- **#001 (dogfood_static_analyzers)** — unblocked. clang-tidy + cppcheck will be added as a separate job in this same workflow.
- **#005 (sarif_reporter_spec)** — when SARIF appears (v0.2), the CI will get an `upload-sarif` step for GitHub Code Scanning.
- **The `v0.1.0` tag** — when it comes to a release, a separate release workflow will appear with a release build and binary publishing.

## Diagnostics

```bash
# Local reproduction of what CI does:
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
cd build/debug && ctest --output-on-failure

# Run under clang instead of gcc (if both are installed):
CC=clang CXX=clang++ cmake -B build/debug-clang -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Status of the latest CI runs:
# https://github.com/blurman-ai/cpparch/actions

# If the cache breaks (rarely) — bump the cache key via a trivial edit to CMakeLists.
```

If CI is red on ubuntu-24.04 but green locally on Astra 1.7 — usually it's:
- a difference in apt-cmake versions (Astra 3.18 vs Ubuntu 3.28) — both must pass the ≥ 3.18 minimum;
- a difference in gcc/clang versions (on CI gcc-13 / clang-18; locally it may be older);
- platform-specific warnings under `-Werror` (need to fix or narrow `-W`).
