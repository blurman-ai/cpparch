# [SCAN] Include scanner — naive line-based extraction

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Difficulty:** S (< 1 day)
**Blocks:** #008c (include_scanner_skip_comments)
**Blocked by:** #008a (include_scanner_api_skeleton)
**Related:** #008 (dependency_graph_foundation)

## Goal

Recognize the simplest `#include "x"` and `#include <x>` in a source file, one
directive per line, without accounting for comments, string literals, or line
continuation.

## Done

- **2026-05-26** — line iteration by scanning for `\n` while counting line_no.
- **2026-05-26** — `skip_ws` for leading whitespace.
- **2026-05-26** — `#include` prefix detector, delimiter choice `"` / `<`, closing delimiter search.
- **2026-05-26** — 7 unit tests for the happy path: quote, angle, multi, no include, leading spaces/tabs, no trailing newline. All green (9/9 overall).

## How it works

`scan_includes(source)` walks `source` character by character, slicing it into logical lines by `\n`. For each line it calls `try_extract(line, line_no, out)`:

1. `skip_ws` — skip leading spaces and tabs.
2. compare the prefix with `#include`; if no match — bail out.
3. `skip_ws` again after the keyword.
4. look at the first character: `"` → `Quote`, `<` → `Angle`, otherwise — bail out.
5. find the closing delimiter (`"` or `>`).
6. assemble `IncludeDirective{kind, token, line_no}` and push it into out.

All steps are regex-free, with no allocations beyond the final `std::string` for the token.

## Controlled by

- No flags / env.
- Behavior is fully determined by the input string.

## Related to

- Same translation unit `src/scan/include_scanner.cpp`, extended in 008c…g.
- Public API unchanged — all 008a clients keep working.

## Diagnostics

- If a directive isn't found, check:
  - whether there's something non-standard between `#` and `include` (not supported, will be added later only if needed);
  - whether the delimiter is closed on the same line (the multi-line case is task 008e).
- Unit tests in `tests/unit/scan/include_scanner_test.cpp` mirror the expectations and serve as a live contract.

## Key decisions

| Decision | Reason |
|---------|---------|
| No regex | Minimum dependencies, parsing is trivial |
| Don't validate token contents | That's the resolver's job in later stages |
| Closing delimiter found via `find`, not a frame-by-frame loop | Shorter and clearer, performance isn't critical |
| Helper `try_extract` returns `bool` | A ready branching point for the future (macro-include in 008g) |

## Changed files

| File | Change |
|------|-----------|
| `src/scan/include_scanner.cpp` | happy-path implementation (line iteration + `try_extract` + `skip_ws`) |
| `tests/unit/scan/include_scanner_test.cpp` | 6 new cases on top of smoke |
