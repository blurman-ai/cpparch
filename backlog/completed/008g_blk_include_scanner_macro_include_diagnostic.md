# [SCAN] Include scanner — macro include as diagnostic

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #008 (dependency_graph_foundation) — closes the scanner branch
**Blocked by:** #008f (include_scanner_first_significant_char)
**Related:** #008 (dependency_graph_foundation)

## Goal

`#include SOMETHING` (macro-based include) does not produce an `IncludeDirective`, but
is returned as a separate `macro_include` diagnostic record.

## Done

- **2026-05-26** — added `enum class DiagnosticKind { MacroInclude }`, `struct IncludeScanDiagnostic`, `struct ScanResult` to `include/archcheck/scan/include_directive.h`.
- **2026-05-26** — the `scan_includes` signature changed: `std::vector<IncludeDirective>` → `ScanResult`.
- **2026-05-26** — `try_extract` heavily reworked: the `"`/`<` branch builds a directive, the `is_ident_start` branch collects an identifier and pushes a diagnostic, otherwise nothing.
- **2026-05-26** — an empty `#include` without a token → nothing returned (fixed as "ignore").
- **2026-05-26** — all 25 existing tests moved to the helper `extract_directives(...)`, 4 new ones added for macro-include. 33/33 green.

## How it works

`try_extract(line, line_no, ScanResult& out)`:

1. `skip_ws` → looks for `#include` → `skip_ws` again.
2. Looks at `line[i]`:
   - `"` → quote include: finds the closing `"`, emits `IncludeDirective{Quote, ...}`.
   - `<` → angle include: finds the closing `>`, emits `IncludeDirective{Angle, ...}`.
   - identifier start (`_` or a letter) → expands to the end of the identifier (`is_ident_cont`), emits `IncludeScanDiagnostic{MacroInclude, "FOO", line_no}`.
   - everything else (including EOL right after `#include`) → return without recording.

`ScanResult` is a simple pair of two vectors. Clients split edges and warnings by purpose: edges go to the graph, diagnostics to the report.

A helper `extract_directives(s) → vector<IncludeDirective>` was added in the tests for backward compatibility with already-written assertions on `.size()` / `[0]`.

## What controls it

- No flags / env. Behavior is fully deterministic from the input string.

## What it relates to

- Public types: `IncludeDirective`, `IncludeKind`, `IncludeScanDiagnostic`, `DiagnosticKind`, `ScanResult` — all in `archcheck::scan::`.
- The future resolver (#012) will consume `ScanResult.directives`; the reporter — `ScanResult.diagnostics`.

## Diagnostics

- If an expected directive suddenly became a diagnostic — check whether quotes/angle brackets were eaten earlier in the pipeline (comments/raw strings).
- If a diagnostic does not appear on `#include FOO` — check `is_ident_start` (no whitespace there that swallowed the start of the identifier).
- `#include 123` is currently ignored (not an identifier, not `"`, not `<`). This is intentional — a token starting with a digit is not a legal preprocessor token and makes no sense.

## Key decisions

| Decision | Reason |
|---------|---------|
| Diagnostic, not a directive, for macro include | A graph edge must be built only from a resolved project include (see §5 mini-design in #008) |
| Return a `ScanResult` struct, not a pair | Future categories (token pasting, conditional includes) will extend `DiagnosticKind` without breaking changes |
| Empty `#include` → nothing | Neither a directive nor a macro form; adding a "MalformedInclude" diagnostic category is unnecessary at this stage |
| Helper `extract_directives` in tests | Less noise in existing assertions on a breaking signature change |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/scan/include_directive.h` | + `DiagnosticKind`, `IncludeScanDiagnostic`, `ScanResult` |
| `include/archcheck/scan/include_scanner.h` | new `scan_includes` signature |
| `src/scan/include_scanner.cpp` | `try_extract` supports the macro-include branch + `is_ident_*` |
| `tests/unit/scan/include_scanner_test.cpp` | helper `extract_directives`, 4 macro-include cases |
