# [SCAN] Include scanner — the rule "`#` is the first significant character"

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** SCAN
**Priority:** blocker
**Difficulty:** S (< 1 day)
**Blocks:** #008g (include_scanner_macro_include_diagnostic)
**Blocked by:** #008e (include_scanner_line_continuation)
**Related:** #008 (dependency_graph_foundation)

## Goal

Recognize `#include` only when `#` is the first significant character of the logical line
(only leading whitespace is allowed).

## Done

- **2026-05-26** — analysis: the rule is already implicitly satisfied by the combination of `skip_ws` + `string_view::compare`. No additional check required.
- **2026-05-26** — added 4 unit tests that pin the behavior explicitly: `int x; #include …`, `; #include …`, two `#include` on one line, splice-joined "code-through-`\`-before-`#include`". 29/29 total count.

## How it works

In `try_extract`:

```cpp
std::size_t i = skip_ws(line, 0);
if (line.compare(i, kIncludeKeyword.size(), kIncludeKeyword) != 0) return false;
```

`skip_ws` advances **only** through spaces/tabs. If the first non-ws character is not `#`, compare fails immediately. Thus:

- `int x; #include "y"` — `skip_ws` stops at `i`, compare sees `int` — return false.
- `; #include "y"` — `skip_ws` stops at `;`, compare sees `; #i…` — return false.
- `#include "a" #include "b"` — compare finds `#include` immediately, then exactly one token is extracted and `try_extract` returns. The second `#include` on the same line is ignored (per the one-directive-per-line logic).
- splice-joined `int x; \\\n#include "y"` — after `join_continuations` the line becomes `int x; #include "y"`, and the first case applies.

## What controls it

- No flags / env.

## What it relates to

- The same `src/scan/include_scanner.cpp`. No code changed, only tests added.

## Diagnostics

- If someone ever adds a `find("#include")` check instead of `compare(i, …)` — the rule will break. The 008f tests won't let that pass.
- If in a logical line `#` is not the first significant character but the scanner still emitted a directive — something in the pipeline went sour: either `skip_ws` got corrupted, or `join_continuations` did something unexpected (CRLF? unicode whitespace?).

## Key decisions

| Decision | Reason |
|---------|---------|
| Don't add an extra explicit check — the `skip_ws + compare` composition is already enough | YAGNI: an extra no-op `if` is garbage |
| Cover the invariant with tests instead of comments in code | Tests document behavior more reliably than prose comments |

## Changed files

| File | Change |
|------|--------|
| `tests/unit/scan/include_scanner_test.cpp` | 4 first-significant-char cases |
