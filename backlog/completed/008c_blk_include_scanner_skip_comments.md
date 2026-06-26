# [SCAN] Include scanner — ignore comments

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Difficulty:** S (< 1 day)
**Blocks:** #008d (include_scanner_skip_string_literals)
**Blocked by:** #008b (include_scanner_naive_extraction)
**Related:** #008 (dependency_graph_foundation)

## Goal

An `#include` inside `//` comments and `/* … */` blocks must not be recognized
as a directive.

## Done

- **2026-05-26** — added a pre-pass `preprocess(source)` that replaces comment contents with spaces while preserving `\n`.
- **2026-05-26** — helpers `consume_line_comment` and `consume_block_comment` for the two kinds of comments.
- **2026-05-26** — `scan_includes` now first calls `preprocess`, then walks the cleaned buffer.
- **2026-05-26** — 5 new unit tests: `//` before `#include`, `//` after `#include`, single-line `/* */`, multi-line block, unclosed `/*`. 14/14 green.

## How it works

`preprocess` is a single-pass, stateless-flag scanner: it looks for the two-character triggers `//` and `/*` and delegates parsing to the helpers.

- `consume_line_comment` runs to `\n`, writing spaces in place of comment characters.
- `consume_block_comment` runs to `*/`, replacing characters with spaces; **`\n` inside the block is preserved** — this is what keeps line numbers correct in the outer `scan_includes`.
- An unclosed `/*` means the entire remaining tail of the source becomes spaces (matching preprocessor behavior).

After `preprocess` the main `scan_includes` loop is unchanged: it still splits into logical lines and calls `try_extract`.

The output buffer has the same length as the input — this is intentional. That way line numbers and offsets in the source match the coordinates in the cleaned buffer, and no extra mapping is needed.

## Controlled by

- No flags. Behavior is entirely determined by the input.

## Related to

- The same file `src/scan/include_scanner.cpp`. The public API did not change.
- The `preprocess` pre-pass will become the extension point for 008d (strings), 008e (line continuation), and 008f (first-significant-char).

## Diagnostics

- If a directive should not have been recognized but was — check that the comment is correctly opened/closed (`*/` must be two characters).
- If a line was "eaten" unexpectedly — most likely an unclosed `/*` earlier in the file.
- At present `preprocess` **does not distinguish strings** — `"// fake"` and `"/* fake */"` inside a string literal are currently treated as comments. This is a known hole, closed in 008d.

## Key decisions

| Decision | Reason |
|----------|--------|
| Pre-pass with output of the same length | Preserves offsets/line numbers, no mapping needed |
| `consume_*` helpers | Keep nesting ≤ 3 levels in the main loop |
| No support for nested `/* /* */ */` | C++ doesn't support them |
| Unclosed `/*` swallows the file tail | Matches preprocessor behavior |

## Changed files

| File | Change |
|------|--------|
| `src/scan/include_scanner.cpp` | `preprocess` + helpers, `scan_includes` uses the cleaned buffer |
| `tests/unit/scan/include_scanner_test.cpp` | 5 cases with comments |
