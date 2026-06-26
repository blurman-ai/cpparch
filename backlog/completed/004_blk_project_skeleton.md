# [BUILD] Project structure and CMake skeleton

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** BUILD
**Priority:** blocker
**Difficulty:** M (one day)
**Blocks:** #002 (github_actions_ci), #001 (dogfood_static_analyzers), all RULES/GRAPH/SCAN tasks
**Blocked by:** ~~#003 (name_availability_check)~~ — unblocked
**Related:** #006 (spec_refactor — roadmap v0.1 defined what we build)

## Goal

Stand up a minimal C++20 CMake skeleton with the layout from the spec, so we can start writing the first module (`config/` or `scan/`).

## Context

Right now the repo has no `.cpp` and no `CMakeLists.txt`. The spec (after #006) prescribes: C++20, CMake, **fast-backend by default (NO libclang in v0.1)**, minimum dependencies (no Boost), ryml + Catch2. Layout from CLAUDE.md: `src/{config,scan,graph,rules,report}/`.

## Execution plan

- [x] Top-level `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.18)` (for Astra 1.7 apt), C++20, warning flags (`-Wall -Wextra -Wpedantic -Werror -Wshadow` / `/W4 /WX`)
- [x] Layout: `src/`, `include/archcheck/`, `tests/`. `fixtures/` and `third_party/` come later.
- [x] YAML: **ryml** v0.7.0 via FetchContent
- [x] Test framework: **Catch2 v3** v3.7.1 via FetchContent
- [x] libclang: NOT wired in v0.1 (fast backend by default per #006); planned for v0.2
- [x] Hello-world `archcheck --version` binary
- [x] Smoke test for the version constants
- [x] `.clang-format` (Allman, 3 spaces, 120 columns)
- [x] `.clang-tidy` (bugprone-*, performance-*, modernize-*, cppcoreguidelines-*, readability-* without noisy checks)
- [x] ~~`CMakePresets.json`~~ — dropped after a real attempt: CMake 3.18 (Astra apt) does not support presets (minimum 3.19). We use explicit `cmake -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`. When contributors show up with a fresher CMake — presets can be brought back.
- [x] Update the CLAUDE.md "Build / test / run" section

## Done

- **2026-05-26** — Skeleton assembled (commit `ea76db9`):
  - Top-level `CMakeLists.txt`: C++20, strict warnings, FetchContent for ryml + Catch2.
  - `src/CMakeLists.txt`: `archcheck::core` (INTERFACE library) + `archcheck` (executable). Version macros via `target_compile_definitions`.
  - `src/main.cpp`: parsing of `--version` / `--help`, exit code 2 on an unknown argument.
  - `include/archcheck/version.h`: `kVersionMajor/Minor/Patch` + `kVersionString` via preprocessor macros from CMake.
  - `tests/CMakeLists.txt` + `tests/smoke_test.cpp`: Catch2 + `catch_discover_tests`.
  - `.clang-format`: Allman, IndentWidth 3, ColumnLimit 120, IncludeBlocks Regroup.
  - `.clang-tidy`: 5 check families, noisy ones disabled explicitly.
  - `CMakePresets.json` v3: debug + release presets with Ninja, debug enables tests, release does not.
- **2026-05-26** — CLAUDE.md updated: the "Build / test / run" section with commands and a layout description.
- **2026-05-26** — **Build run and verified** on Astra Linux 1.7 (gcc, ninja 1.11.1, cmake 3.18.4):
  - `cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug` — configuration passed, ryml + Catch2 pulled into `build/_deps/`.
  - `cmake --build build/debug` — 138 targets built without warnings on our code (with `-Werror -Wshadow`).
  - `./build/debug/src/archcheck --version` → `archcheck 0.1.0` ✓
  - `./build/debug/src/archcheck --help` → usage ✓
  - `./build/debug/src/archcheck broken` → exit code **2** + usage ✓
  - `ctest --output-on-failure`: 2/2 PASSED (5 assertions).
  - During verification: `CMakePresets.json` removed (incompatible with CMake 3.18), `cmake_minimum_required` lowered from 3.21 to 3.18, the CLAUDE.md "Build" section rewritten for explicit cmake commands.

## Key decisions

| Decision | Reason |
|---------|---------|
| YAML: **ryml** v0.7.0 | header-only-ish, low-allocation, no Boost — fits "single static binary". The spec lists it first. |
| Test: **Catch2 v3** v3.7.1 | Simple integration, ergonomics for fixture-oriented tests, no need for gmock. |
| Deps: **FetchContent** | The standard for OSS without vcpkg/conan. First build online, then offline. CI caches `build/_deps/`. |
| Generator: **Ninja** in presets | Faster than Make, available on all platforms. |
| Build: Debug by default | CLAUDE.md: "don't run Release without an explicit request". ARCHCHECK_BUILD_TESTS=ON in debug, OFF in release. |
| `archcheck::core` as INTERFACE | In v0.1 there is no implementation; subsystems (config/scan/graph/rules/report) are wired in as they arrive. |
| `cmake_minimum_required` 3.21 | `CMakePresets.json` v3 requires 3.21. 3.20 does not give presets v3. |
| Project version = 0.1.0 | Consistent with roadmap v0.1 in the spec. SemVer pre-1.0. |
| libclang NOT wired | Per #006 — fast backend = default in v0.1. libclang arrives in v0.2 via `--with-clang`. |

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| `CMakeLists.txt` | new — top-level, C++20, FetchContent ryml | `ea76db9` |
| `src/CMakeLists.txt` | new — archcheck_core + archcheck binary | `ea76db9` |
| `src/main.cpp` | new — hello-world with `--version` / `--help` | `ea76db9` |
| `include/archcheck/version.h` | new — version constants | `ea76db9` |
| `tests/CMakeLists.txt` | new — Catch2 + catch_discover_tests | `ea76db9` |
| `tests/smoke_test.cpp` | new — 2 smoke tests for the version | `ea76db9` |
| `.clang-format` | new — Allman, 3 spaces, 120 cols | `ea76db9` |
| `.clang-tidy` | new — narrow starter set | `ea76db9` |
| `CMakePresets.json` | new — debug + release presets | `ea76db9` |
| `CLAUDE.md` | "Build / test / run" section filled in | (current) |
| `CHANGELOG.md` | entry about the skeleton | (current) |

## How it works

The CMake configuration is three-tiered:

1. **Top-level `CMakeLists.txt`** — sets the project, C++20 standard, warning flags, options (`ARCHCHECK_BUILD_TESTS`, `ARCHCHECK_ENABLE_WARNINGS`), pulls in dependencies via FetchContent, delegates to `src/` and `tests/`.

2. **`src/CMakeLists.txt`** — defines two targets:
   - `archcheck_core` (alias `archcheck::core`) — INTERFACE library, aggregates include paths, version macros, ryml. The subsystems (`config/`, `scan/`, `graph/`, `rules/`, `report/`) will be wired in here as PRIVATE sources via `target_sources()` as the corresponding tasks arrive.
   - `archcheck` — executable, links against `archcheck::core`. Currently only `main.cpp` with `--version` / `--help`.

3. **`tests/CMakeLists.txt`** — Catch2 v3 via FetchContent, `catch_discover_tests` registers individual `TEST_CASE`s in CTest.

**Dependencies are pulled on the first `cmake -B build`** into `build/_deps/`:
- `ryml-src` / `ryml-build` — YAML parser.
- `catch2-src` / `catch2-build` — test framework.

After the first build both live in the cache — subsequent configurations are offline.

**Version macros** are passed through `target_compile_definitions` on `archcheck_core`, the header `include/archcheck/version.h` reads them and wraps them in `constexpr int / const char*`.

## What controls it

| Parameter | Where it changes |
|---|---|
| Project version | `CMakeLists.txt` → `project(archcheck VERSION ...)`. Also pinned by the `v0.1.0` tag at release. |
| Enable/disable tests | `cmake -DARCHCHECK_BUILD_TESTS=OFF` or preset `release` |
| Strict warnings | `-DARCHCHECK_ENABLE_WARNINGS=OFF` — disables -Werror and company (for downstream packaging) |
| Build type | `--preset debug` or `--preset release`. Or classically `-DCMAKE_BUILD_TYPE=Debug` |
| Code style | `.clang-format` — `clang-format -i src/*.cpp` |
| Static analysis | `.clang-tidy` — `clang-tidy -p build/debug src/main.cpp` |

## What it relates to

- **#002 (github_actions_ci)** — can now start. The CI workflow references the `debug` presets; it will add `cmake --preset debug`, `cmake --build --preset debug`, `ctest --preset debug` steps, plus `actions/cache` for `build/_deps/`.
- **#001 (dogfood_static_analyzers)** — can now start. It will run clang-tidy / cppcheck / lizard on `src/` and `include/` (third_party folders in `build/_deps/` are filtered out in `.clang-tidy`).
- **Future RULES/GRAPH/SCAN tasks** — wire their source files into `archcheck::core` via `target_sources(archcheck_core PRIVATE ...)`.
- **#005 (sarif_reporter_spec)** — will add `src/report/sarif_reporter.{h,cpp}` (but that is v0.2+).
- **#006 (spec_refactor)** — laid down roadmap v0.1, on which this skeleton is built (fast-backend-first, without libclang).

## Diagnostics

Commands for verification (until the build is run by the user):

```bash
# Configuration (requires internet on the first run — pulls ryml + Catch2):
cmake --preset debug

# Build:
cmake --build --preset debug

# Run tests:
ctest --preset debug

# Run the binary:
./build/debug/src/archcheck --version
# Expected: "archcheck 0.1.0"

./build/debug/src/archcheck --help
# Expected: usage with a description of available commands

./build/debug/src/archcheck unknown
# Expected: exit code 2 + error message

# Check the linter config:
clang-format --version    # >= 16
clang-tidy --version      # any modern one

# compile_commands.json should be present:
ls build/debug/compile_commands.json
```

If the first build fails on FetchContent — check:
- is there internet?
- is port 443 on github.com reachable?
- `git --version >= 2.17`?
- (if a corporate proxy) are `https_proxy` / `http_proxy` set?
