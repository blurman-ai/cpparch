# [SCAN] Include resolver — zero-config repo-local

**Creation date:** 2026-05-26
**Start date:** 2026-05-26
**Completion date:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Difficulty:** M (1-2 days)
**Blocks:** #015 (diff_primitives), #017 (graph_fixtures)
**Blocked by:** #008g (include_scanner_macro_include_diagnostic), #011 (discover_files_and_indexes)
**Related:** #008 (dependency_graph_foundation)

## Goal

Turn `IncludeDirective` + project indexes into a `ResolvedInclude` of one of
four categories: `project` / `external` / `unresolved` / `ambiguous`.

## Context

See the §4 mini-design in #008. This is the heart of the zero-config extraction layer: it's exactly
here that the decision is made whether an include lands in the graph as an edge.

## Done

- **2026-05-26** — created `include/archcheck/scan/resolved_include.h` (`Resolution`, `ResolvedInclude`).
- **2026-05-26** — created `include/archcheck/scan/include_resolver.h` (declarations of `resolve_include` / `resolve_includes`).
- **2026-05-26** — implemented `src/scan/include_resolver.cpp` with the quote/angle resolution algorithm.
- **2026-05-26** — added unit test `tests/unit/scan/include_resolver_test.cpp` (12 cases: 5 quote branches, 4 angle, plus tie-break + batch).
- **2026-05-26** — `archcheck_core` picks up the new `.cpp`, `archcheck_tests` — the new test source.
- **2026-05-26** — Debug build green, `ctest` 75/75, `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — clean.

## How it works

The public API sits in `include/archcheck/scan/`:
- `Resolution` — `Project` / `External` / `Unresolved` / `Ambiguous`.
- `ResolvedInclude` — `{ directive, source_file, resolution, target, candidates }`.
  `target` is valid only on `Project`; `candidates` is filled only on `Ambiguous`.
- `resolve_include(directive, source_file, files, index) -> ResolvedInclude` — a single resolve.
- `resolve_includes(directives, source_file, files, index) -> std::vector<ResolvedInclude>` — batch.

Algorithm (one pass, no backtracking):

**Quote `#include "x"`:**
1. Glue `dir(source_file) + x` → exact match by `exact_path_index`.
2. If none — exact match by `x` directly.
3. If none — `suffix_index[x]`:
   - 0 candidates → `Unresolved`;
   - 1 candidate → `Project`;
   - 2+ → `Ambiguous` (`candidates` = the whole vector).

**Angle `#include <x>`:**
1. Exact match by `x` via `exact_path_index`.
2. If none — `suffix_index[x]`:
   - 0 candidates → `External` (a system header — normal);
   - 1 candidate → `Project`;
   - 2+ → `Ambiguous`.

Deduplication of `(source, target)` edges is deliberately NOT done at the
resolver level: that's the work of the graph builder (#014), which holds the contract
"one logical edge". The resolver, in turn, must preserve all directives with their
`line`, so that it can later report "duplicate at line N".

## What controls it

- No flags / environment variables.
- The behavior is fully deterministic: `ProjectIndex` is built sorted
  via `build_project_index`, suffix candidates go in the same order as
  the file ids in `discover_files` (which sorts by `path`).

## What it's connected to

- The direct consumer is the future graph builder (#014), which will turn
  `ResolvedInclude::Project` into a DAG edge, and put the other categories into
  diagnostics.
- Uses `ProjectIndex::exact_path_index` and `suffix_index` from #011.
- Accepts `IncludeDirective` from the scanner (#008a—#008g).
- The `files` parameter in the API is left for symmetry and future extensions
  (for example, a reverse lookup `NodeId -> ProjectFile`), but the current
  implementation doesn't need it.

## Diagnostics

- If a quote include unexpectedly resolves as `Unresolved`: check
  that `source_file` is passed as a **repo-relative POSIX path** (not absolute,
  not with `\\`), otherwise `dir(source_file) + token` will assemble garbage.
- If a set of different `.h` fall into `External`: possibly the files dropped out
  of `discover_files` (excluded by directory name or extension —
  see the §1 mini-design in #008).
- `Ambiguous` is not a bug, but a signal. In zero-config v0.1, namesake files
  in different directories should be either renamed or resolved
  by an explicit include path (a feature beyond v0.1).

## Key decisions

| Decision | Reason |
|---------|---------|
| `angle` without a project match → `external`, not `unresolved` | Matches the usual expected semantics of system headers |
| Don't try to distinguish `external` from a "forgotten dependency" | That's the work of a separate future layer (vendored / external manifest) |
| `(source, target)` deduplication handed to the graph builder | The resolver must preserve all directives with line; dedup — a property of the graph |
| The `files` parameter is accepted but unused | API symmetry with future lookup scenarios; zero cost, an explicit signal "files will be here" |
| The implementation is split into helpers (`source_directory`, `find_*`, `make_*`, `resolve_quote`, `resolve_angle`, `resolve_by_suffix`) | Lizard thresholds (≤ 30 lines / CCN ≤ 15) + readability; each function is responsible for one step of the algorithm |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/scan/resolved_include.h` | new |
| `include/archcheck/scan/include_resolver.h` | new |
| `src/scan/include_resolver.cpp` | new |
| `tests/unit/scan/include_resolver_test.cpp` | new (12 cases) |
| `src/CMakeLists.txt` | + `scan/include_resolver.cpp` |
| `tests/CMakeLists.txt` | + `unit/scan/include_resolver_test.cpp` |
