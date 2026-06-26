# [RULES] SF.7: block /* */ comments are not stripped → false positives in Doxygen \code

**Created:** 2026-05-28
**Started:** —
**Status:** completed
**Module:** RULES
**Priority:** major
**Difficulty:** S
**Blocks:** —
**Blocked by:** —
**Related:** #035 (sf7_brace_depth_tracking), #028 (rules_engine_mvp)

## Goal

SF.7 must not report `using namespace` inside `/* ... */` comments.

## Context

A run on folly (commit `acc9ce5`) revealed false positives in Doxygen comments:

```cpp
// folly/FixedString.h:447 — inside /** \code ... \endcode */:
 * \code
 *   using namespace folly;           ← line with "using namespace", but it is a comment!
 *   return makeFixedString("****");
 * \endcode
```

```cpp
// folly/FixedString.h:2897 — likewise:
 * \code
 * using namespace folly::string_literals;
 * \endcode
```

The current implementation `sf7_using_namespace.cpp` strips only `//` single-line comments (`stripLineComment`). Multi-line `/* ... */` blocks are not filtered — any line inside a block comment with `using namespace` produces a violation.

The pattern is common in documented libraries (folly, LLVM, Abseil): authors show usage examples in Doxygen `@code`/`\code` blocks.

## Solution

Add `block_comment_depth` tracking in `scanFile` alongside `brace_depth` (task #035):

```cpp
bool inBlockComment = false;
for (each line) {
    // toggle block comment state
    update inBlockComment based on /* and */
    if (inBlockComment) continue;
    // existing logic: brace_depth + hasUsingNamespace
}
```

Limitation: the line-by-line approach does not handle `/* ... */` on a single line correctly without an FSM. Sufficient accuracy — exclude lines where `/*` is open and `*/` is not yet closed.

## Combining with #035

Tasks #035 and #038 modify the same file `sf7_using_namespace.cpp`. It makes sense to do them together in one PR.

## Plan

- [ ] `sf7_using_namespace.cpp`: add `inBlockComment` state to `scanFile`
- [ ] Verify: folly/FixedString.h → 0 false positives in `\code` blocks
- [ ] Verify: fmt, spdlog, Catch2, abseil — no regressions
- [ ] Fixture: `fixtures/sf7/pass_using_in_block_comment/`
- [ ] Unit test: `using namespace` in `/* ... */` → pass

## Done

- Implemented together with #035 in a single commit `71e4fa3`
- `updateBlockCommentState()` handles `/* */` blocks; lines inside a comment return an empty `string_view`
- Test: `using namespace` in a Doxygen `\code` block → no violation

## Changed files

| File | Change |
|------|--------|
| `src/rules/sf7_using_namespace.cpp` | `inBlockComment` state in `updateBlockCommentState()` |
| `tests/unit/rules/sf7_using_namespace_test.cpp` | block comment tests |
| `fixtures/sf7_using_namespace/pass_using_in_block_comment/a.h` | new |
