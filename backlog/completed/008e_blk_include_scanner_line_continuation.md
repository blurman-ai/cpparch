# [SCAN] Include scanner — logical-line continuation

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Difficulty:** S (< 1 day)
**Blocks:** #008f (include_scanner_first_significant_char)
**Blocked by:** #008d (include_scanner_skip_string_literals)
**Related:** #008 (dependency_graph_foundation)

## Goal

Splice lines ending in `\` + EOL into a single logical line before
lexical analysis.

## Done

- **2026-05-26** — introduced `struct Joined { text, line_of_offset }` and a `join_continuations` pre-pass that removes `\\\n` and preserves the offset → physical line mapping.
- **2026-05-26** — `scan_includes` now flows `source → join_continuations → preprocess → line iteration`, with line_no taken via `line_at(joined, line_start)`.
- **2026-05-26** — 6 unit tests: splice inside `#include`, between keyword and token, inside the token, several in a row, `\` without `\n` (not a continuation), preservation of physical numbering after splice. 25/25 overall.

## How it works

`join_continuations(source) -> Joined`:
- walks byte by byte;
- if `source[i]=='\\'` and `source[i+1]=='\n'` — both characters are dropped, the physical line increments, output does not grow;
- otherwise the character is copied into `Joined::text`, and its physical line is stored in `Joined::line_of_offset[offset]`.

`Joined::line_of_offset` is a `vector<int>`, one element per byte of `text`. This gives O(1) lookup of the physical line by offset, later used in `line_at`.

`scan_includes`:
1. `joined = join_continuations(source)`.
2. `cleaned = preprocess(joined.text)` (comments + raw strings, same length).
3. Walk `cleaned` by `\n` — for each segment call `try_extract(segment, line_at(joined, line_start))`. This way the directive reports the physical line of the first character of the logical line.

## Controlled by

- No flags / env.

## Related to

- Same `src/scan/include_scanner.cpp`.
- Public API unchanged — 008a clients keep working.

## Diagnostics

- If a directive reports the "wrong" line — check whether there was a splice above; `line_at` takes the physical line of the first character of the logical line (which matches user expectations).
- CRLF (`\\\r\n`) is not supported yet — a known limitation, normalization will land in #011 (file reader).
- Splice is applied inside raw strings too (which technically diverges from the C++ phase-2 special rule), but for include detection this is harmless: raw string contents are swallowed by `consume_raw_string` anyway. Documented as a known simplification.

## Key decisions

| Decision | Reason |
|---------|---------|
| A separate `join_continuations` pre-pass, not integrated into `preprocess` | Linear and readable; the physical-line mapping is conveniently stored alongside text |
| `line_of_offset` as `vector<int>` | 4 bytes of memory per source byte is acceptable for v0.1; alternatives (RLE, binary search over ranges) are premature |
| Splice applied inside raw strings too | A simplification; produces no false-positives in the current grammar |
| No `\\\r\n` support | For now the scanner always receives an already-normalized LF source; CRLF is the file reader's job |

## Changed files

| File | Change |
|------|-----------|
| `src/scan/include_scanner.cpp` | `Joined` / `join_continuations` / `line_at`, new `scan_includes` pipeline |
| `tests/unit/scan/include_scanner_test.cpp` | 6 continuation cases |
