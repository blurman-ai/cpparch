# [SCAN] Include scanner — ignore string literals

**Date created:** 2026-05-26
**Date started:** 2026-05-26
**Date completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #008e (include_scanner_line_continuation)
**Blocked by:** #008c (include_scanner_skip_comments)
**Related:** #008 (dependency_graph_foundation)

## Goal

`#include` inside `"…"`, `'…'`, and raw string literal `R"delim(…)delim"` must not
be recognized as a directive.

## Done

- **2026-05-26** — added `consume_raw_string`, supporting empty and custom delimiter.
- **2026-05-26** — `preprocess` catches `"` immediately after `R` and switches to raw-string consume.
- **2026-05-26** — 5 unit tests: ordinary string, char literal, single-line `R"(…)"`, custom delimiter `R"d(…)d"`, multi-line raw string. 19/19 overall count green.

## How it works

A branch was added to `preprocess`: if the current character is `"` and the previous character was `R`, this is the start of a raw string. `consume_raw_string`:

1. Reads the delimiter — everything between `"` and `(`.
2. Replaces the contents of the raw string with spaces (newlines are preserved → line numbers stay correct).
3. The closing sequence is assembled as `")" + delim + "\""` and searched for via `string_view::compare`.

Ordinary string `"…"` and char literal `'…'` are **not stripped** — and this is intentional. Analysis:

- `#include` is caught only when `#` is the first significant character on the line.
- In `const char* s = "#include \"x\"";` the `#` character is positioned after `"`, not first on the line → not a false positive.
- An ordinary string cannot contain a "fresh line beginning with #include", because ordinary strings do not span across `\n` (only via line continuation — that's 008e).

So in this subtask only raw-string consume is relevant; for `"…"` and `'…'` it is sufficient that, in its current form, the scanner physically cannot trigger on them.

## What controls it

- No flags / env.

## What it relates to

- The same implementation in `src/scan/include_scanner.cpp`.
- 008e (line continuation) will add an extra nuance: a continuation inside an ordinary string turns it into a multi-line one, and then an ordinary-string snippet with `#include` after `\` may spill onto the next line. This is resolved in 008e via a pre-pass continuation.

## Diagnostics

- If `#include` is caught inside a raw string — check that the delimiter in `R"d(…)d"` actually matches, and that `)d"` is not broken up.
- If a raw string "did not eat" the text — check that `"` comes immediately after `R` (no spaces).
- At the moment `'1'` (the digit-separator prefix is in fact rare and is ignored in the current preprocess as "not stripped") — record as an edge case for future v0.x, if anyone needs it. For now, no such scenarios in C++ code that would break the scanner have been encountered.

## Key decisions

| Decision | Reason |
|---------|---------|
| Full raw strings support | In archcheck tests this will come up often, and in real code too (ABI docs, JSON-embedded, etc.) |
| Ordinary string / char literal not stripped | Cannot produce a false positive in the scanner's current grammar; saves LOC and no risk of breaking `#include "x"` |
| `R` without prefix check (`u8R`, `LR`, …) | `R` + `"` is a sufficient signal; identifier-suffix cases are extremely rare and easily handled later |

## Changed files

| File | Change |
|------|-----------|
| `src/scan/include_scanner.cpp` | `consume_raw_string` + dispatch in `preprocess` |
| `tests/unit/scan/include_scanner_test.cpp` | 5 new cases for strings / chars / raw |
