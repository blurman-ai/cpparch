# [SCAN][FIXTURES] Include scanner — integration fixtures from disk

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN, FIXTURES
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** —
**Blocked by:** #008g (include_scanner_macro_include_diagnostic)
**Related:** #008 (dependency_graph_foundation), #017 (graph_fixtures)

## Goal

Pin down the include-scanner's behavior on real files from disk, not just on
inline strings in unit tests.

## Context

After 008a…g the scanner is fully covered by unit tests (33 cases). But the
scanner's real client will read `*.h` / `*.cpp` from disk via the resolver and
discovery (#011/#012). To make sure that `scan_includes(std::string_view)`
doesn't suffer from differences between "inline literal vs file content" (BOM, trailing
whitespace, encoding, binary reads), an integration layer is needed: the fixtures
live as separate files in `fixtures/scan/include_scanner/`, the test reads them and
runs them through `scan_includes`.

This does **not** turn the scanner into a rule (rules have the `pass/` + `fail_*/` format per
MVP.md). The scanner is an extraction primitive, so the fixtures are flat: one .cpp
per scenario.

## Done

- **2026-05-26** — `fixtures/scan/include_scanner/` with 6 files:
  - `simple.cpp` — angle + quote + angle.
  - `comments.cpp` — `//` and multiline `/* */`.
  - `string_literal.cpp` — `#include` inside an ordinary string.
  - `raw_string.cpp` — `#include` inside `R"(…)"`.
  - `continuation.cpp` — `\\\n` inside `#include`.
  - `macro_include.cpp` — `#include CONFIG_HEADER` → diagnostic.
- **2026-05-26** — `tests/integration/scan/scanner_fixtures_test.cpp` — 6 TEST_CASE, read the file and check specific expectations on `directives` and `diagnostics`.
- **2026-05-26** — `tests/CMakeLists.txt` wires in the integration test source, passes `ARCHCHECK_FIXTURES_DIR` as a compile definition, and adds `stdc++fs` linking for GCC < 9 (system g++ 8.3 in the dev environment).
- **2026-05-26** — Debug build clean, ctest 39/39 green.

## How it works

The test source stores the absolute path to the fixtures via the macro
`ARCHCHECK_FIXTURES_DIR`, passed through by CMake:

```cmake
target_compile_definitions(archcheck_tests PRIVATE
   ARCHCHECK_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/fixtures"
)
```

In the test helper:

```cpp
std::filesystem::path fixture(std::string_view name)
{
   return std::filesystem::path{ARCHCHECK_FIXTURES_DIR} / "scan" / "include_scanner" / name;
}
```

`read(p)` opens the file in `std::ios::binary` and pulls everything through
`istreambuf_iterator`. The result is fed into `scan_includes`. After that — the
usual `REQUIRE` on size, kind, token, line.

## What controls it

- `ARCHCHECK_FIXTURES_DIR` — set by CMake, the single "magic" path.
- No env vars / runtime flags.

## What it relates to

- The scanner's public API — unchanged.
- The CMake target `archcheck_tests` now also links against `stdc++fs` (on GCC ≥ 9 — a no-op).
- When the resolver (#012) arrives, these same fixtures can be reused as a "small project" for testing discovery.

## Diagnostics

- If `REQUIRE(f.is_open())` fails — check that `ARCHCHECK_FIXTURES_DIR` points to the right path (the CMake cache may be stale, `cmake --build` after `cmake -B` saves the day).
- If on a new machine the build won't link against `<filesystem>` — update the CMake GCC version check.
- If a new fixture "isn't found" — check the name in `fixture("...")`, it must match the file verbatim.

## Key decisions

| Decision | Rationale |
|---------|-----------|
| Flat structure `fixtures/scan/include_scanner/*.cpp` | The scanner is a primitive, not a rule; the `pass/`/`fail_*/` hierarchy is excessive for it |
| One TEST_CASE per fixture | More readable than data-driven; each fixture answers a specific question |
| `ARCHCHECK_FIXTURES_DIR` as a compile def | No need to drag along an env var and set it in CI; CMake already knows the repo root |
| Binary read via `istreambuf_iterator` | No text-mode newline conversion (Windows would break the line-number expectations) |
| `stdc++fs` support via CMake version check | g++ 8.3 in the dev environment requires it, GCC ≥ 9 loses nothing |

## Changed files

| File | Change |
|------|--------|
| `fixtures/scan/include_scanner/simple.cpp` | new |
| `fixtures/scan/include_scanner/comments.cpp` | new |
| `fixtures/scan/include_scanner/string_literal.cpp` | new |
| `fixtures/scan/include_scanner/raw_string.cpp` | new |
| `fixtures/scan/include_scanner/continuation.cpp` | new |
| `fixtures/scan/include_scanner/macro_include.cpp` | new |
| `tests/integration/scan/scanner_fixtures_test.cpp` | new |
| `tests/CMakeLists.txt` | + integration source, + `ARCHCHECK_FIXTURES_DIR`, + `stdc++fs` link for GCC<9 |
