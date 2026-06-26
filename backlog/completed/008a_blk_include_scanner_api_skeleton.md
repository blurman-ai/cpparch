# [SCAN] Include scanner — API skeleton

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #008b (include_scanner_naive_extraction)
**Blocked by:** —
**Related:** #008 (dependency_graph_foundation)

## Goal

Fix the public shape of the textual include scanner: data types and the function
signature, without implementing parsing logic.

## Done

- **2026-05-26** — created `include/archcheck/scan/include_directive.h` (`IncludeKind`, `IncludeDirective`).
- **2026-05-26** — created `include/archcheck/scan/include_scanner.h` (declaration of `scan_includes`).
- **2026-05-26** — `src/scan/include_scanner.cpp` with a stub returning an empty vector.
- **2026-05-26** — `archcheck_core` switched from INTERFACE to STATIC, added `scan/include_scanner.cpp`.
- **2026-05-26** — smoke test `tests/unit/scan/include_scanner_test.cpp`, wired into `tests/CMakeLists.txt`.
- **2026-05-26** — Debug build green, `ctest` 3/3 (including the new case).

## How it works

The public API sits in `include/archcheck/scan/`:
- `IncludeKind` — `Quote` / `Angle`, distinguishes the directive form.
- `IncludeDirective` — `{ kind, token, line }`; `token` without the surrounding quotes / angle brackets.
- `scan_includes(std::string_view source) -> std::vector<IncludeDirective>` — the single entry point.

The implementation at this stage is a stub: the input is swallowed, an empty vector is returned. Parsing semantics grow in 008b…008g without changing the signature.

## What controls it

- No flags / environment variables at this stage.
- Wired in through `archcheck::core` (CMake target).

## What it relates to

- The `scan/` subsystem — the first file in `src/scan/`.
- Depends only on the STL.
- The `archcheck_tests` test target now links against `archcheck::core` and sees the public `scan/` headers.

## Diagnostics

- If the linker can't find `scan_includes` — check that `src/scan/include_scanner.cpp` is in the list of source files of `archcheck_core` in `src/CMakeLists.txt`.
- If the test can't find the header — check that the repo-root/include path is added to `target_include_directories(archcheck_core PUBLIC …)`.

## Key decisions

| Decision | Rationale |
|---------|-----------|
| The scanner returns only "raw" directives | Resolution is a separate pipeline stage, see §4 mini-design in #008 |
| `int line` (not `line+column`) | column isn't used by any rule in v0.1, add it later if needed |
| `archcheck_core` STATIC, not INTERFACE | A real implementation appeared, an INTERFACE target can't hold it |
| `target_include_directories` via `PUBLIC` | Tests should see the public headers without explicitly duplicating paths |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/scan/include_directive.h` | new |
| `include/archcheck/scan/include_scanner.h` | new |
| `src/scan/include_scanner.cpp` | new (stub) |
| `src/CMakeLists.txt` | INTERFACE → STATIC, added `scan/include_scanner.cpp` |
| `tests/unit/scan/include_scanner_test.cpp` | new (smoke) |
| `tests/CMakeLists.txt` | wired in the new test source |
