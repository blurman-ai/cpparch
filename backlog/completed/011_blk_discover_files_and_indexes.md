# [SCAN] Discover files + project indexes

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #012 (include_resolver)
**Blocked by:** —
**Related:** #008 (dependency_graph_foundation)

## Goal

Walk the project root and collect the set of project files, plus build
`exact_path_index` and `suffix_index`, which the resolver will need.

## Context

See §1 and §4 of the mini-design in #008. Without discovery and the indexes the resolver
cannot work at all. This subtask is the next layer above the textual scanner (#008a…g),
but independent of it: the scanner works with a single file, discovery works with the whole
repo.

## Done

- **2026-05-26** — created `include/archcheck/scan/project_files.h`: the types
  `ProjectFile`, `ProjectIndex`, `NodeId = std::size_t`, and the declarations
  `discover_files(root)` and `build_project_index(files)`.
- **2026-05-26** — implemented `src/scan/project_files.cpp`:
  `recursive_directory_iterator` skipping excluded directories, a filter by
  extensions, POSIX normalization via `path::generic_string()`,
  deterministic sorting of the result.
- **2026-05-26** — implemented `build_project_index`: an exact-path map +
  a suffix map by `/` segments (via incremental search for the next `/`).
- **2026-05-26** — `src/CMakeLists.txt`: `scan/project_files.cpp` added to the
  `archcheck_core` source list.
- **2026-05-26** — `tests/CMakeLists.txt`: hooked up
  `unit/scan/project_files_test.cpp`.
- **2026-05-26** — wrote a unit test for 12 cases: empty directory, full
  set of extensions, excluded directories (`.git`, `build`, `cmake-build-*`,
  `out` and others), no auto-exclusion of `third_party/`/`vendor/`,
  POSIX normalization, deterministic sorting, exact lookup, full
  suffix set, multi-candidate (collision), single-segment path.
- **2026-05-26** — Debug build green, `ctest` 51/51 (39 old + 12 new).
- **2026-05-26** — `lizard --CCN 15 --length 30 --arguments 5 --warnings_only`
  clean.

## How it works

`discover_files(root)`:
- Uses `std::filesystem::recursive_directory_iterator` with
  `disable_recursion_pending()` to skip excluded directories.
- Excluded directory names: `.git`, `build`, `.cache`, `.idea`, `.vscode`,
  `out`, plus any with the prefix `cmake-build-`.
- Accepted extensions (12 of them, see §1 of the mini-design in #008):
  `.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .tpp .inl .inc`.
- Paths are normalized via `path::generic_string()` — `/` on all platforms,
  including Windows.
- All errors go through `std::error_code` (we don't throw exceptions).
- A final sort by `path` — determinism for CI.

`build_project_index(files)`:
- `NodeId` is just the index in `std::vector<ProjectFile>`, which gives O(1)
  back-lookup into files without a separate structure.
- `exact_path_index`: full repo-relative POSIX path → id.
- `suffix_index`: each `/`-segment suffix of a path → a list of candidate ids
  (`a/b/c.h` → `{"a/b/c.h", "b/c.h", "c.h"}`). Collisions (the same file name
  in different directories) form a multi-candidate vector — the resolver itself
  decides how to deal with it (see §4 of the mini-design).

## Controlled by

- No flags / environment variables at this stage.
- The list of extensions and the list of excluded directories are `constexpr` arrays in
  the `.cpp`. If configuration is needed, we factor them into a struct later.

## Related to

- The direct consumer is the future `include_resolver` (#012). The resolver receives
  the `ProjectIndex` and resolves an `IncludeDirective` into a `NodeId` or
  `external`/`ambiguous`/`unresolved`.
- In parallel with the textual scanner (#008a…g): the scanner works per-file,
  discovery works per-repo. There is no direct dependency between them in code.
- `NodeId` is deliberately introduced locally (`std::size_t`) — when a
  full-fledged graph module (#008) appears, the type can be lifted into a common header without
  changing the public discovery API.

## Diagnostics

- If files from `build/` or `.git/` appear in the output — check that the directory
  name arrives in `entry.path().filename()` exactly (we compare the
  POSIX name without a trailing slash).
- If `\` shows up in paths on Windows — check that
  `generic_string()` is used, not `string()`.
- If the result order is unstable — check that the final `std::sort`
  was not accidentally removed (without it, ctest may flicker on CI).
- The suffix index doesn't contain the key `c.h` for the file `a/b/c.h`? Check that
  the loop over `start` begins at `0` and advances by `slash + 1`, not
  `slash`.

## Key decisions

| Decision | Reason |
|----------|--------|
| Suffix index by `/` segments, not by substrings | Matches how `#include` works — a path, not an arbitrary substring |
| No auto-exclusion of `third_party/` | See §1 of the mini-design: we don't silently hardwire heuristics |
| `NodeId = std::size_t` locally in `scan/` | The `graph/` module doesn't exist yet (#008); we lifted the type when it was needed — without breaking the API |
| `ProjectIndex` as a struct, not two free builders | One object — one lifetime, one handoff point to the resolver; no chance of forgetting one of the indexes |
| Deterministic sorting in `discover_files` | The "same input → same output" contract from the spec; CI should not flicker because of FS traversal order |
| `recursive_directory_iterator` + `error_code` overload | No exceptions on the hot path; FS errors don't crash the whole scan |
| POSIX via `generic_string()` | One path format on all platforms — the indexes must compare byte-for-byte |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/scan/project_files.h` | new |
| `src/scan/project_files.cpp` | new |
| `tests/unit/scan/project_files_test.cpp` | new |
| `src/CMakeLists.txt` | added `scan/project_files.cpp` to `archcheck_core` |
| `tests/CMakeLists.txt` | linked `unit/scan/project_files_test.cpp` |
