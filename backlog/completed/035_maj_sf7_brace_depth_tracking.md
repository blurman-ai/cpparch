# [RULES] SF.7: brace `{}` depth is not tracked → false positives inside functions and lambdas

**Created:** 2026-05-28
**Started:** —
**Status:** completed
**Module:** RULES
**Priority:** major
**Difficulty:** S
**Blocks:** —
**Blocked by:** —
**Related:** #028 (rules_engine_mvp)

## Goal

SF.7 should report only `using namespace` at global or namespace scope, not inside function bodies, method bodies, or lambdas.

## Context

A run on Catch2 (commit `69e0473`) surfaced false positives. Two specific patterns (verified by hand):

**Pattern 1 — `using namespace` inside a method body:**
```cpp
// catch_tostring.hpp:270
template<>
struct StringMaker<bool> {
    static std::string convert(bool b) {
        using namespace std::string_literals;  // SF.7 fires — false positive
        return b ? "true"s : "false"s;
    }
};
```

**Pattern 2 — `using namespace` inside a lambda in a macro:**
```cpp
// catch_generators.hpp:251
#define GENERATE(...) \
    Catch::Generators::generate(...,
      [](){ using namespace Catch::Generators; return makeGenerators(__VA_ARGS__); })
                    // ^^^ SF.7 fires — false positive, lambda is not global scope
```

The current implementation (`sf7_using_namespace.cpp`) is a line-by-line search for `using namespace` with no `{}`-depth tracking. Any line containing this pattern produces a violation.

## Solution

Add a `{}` nesting-depth counter to `scanFile`. Report `using namespace` only when `brace_depth == 0`.

```cpp
int braceDepth = 0;
for (each line) {
    braceDepth += count('{') - count('}');
    if (braceDepth == 0 && hasUsingNamespace(line))
        report();
}
```

Limitations of this approach:
- Counting `{}` from strings is not a real parser: strings and comments can contain braces. For most real-world headers this is good enough. Full parsing is libclang (a separate topic, v0.2).
- Multi-line expressions (a lambda spanning several lines) are handled correctly — the counter rises and falls properly.

## Plan

- [ ] `sf7_using_namespace.cpp`: add a `braceDepth` counter to `scanFile`
- [ ] Verify: Catch2 → 0 false SF.7 in `catch_tostring.hpp` and `catch_generators.hpp`
- [ ] Verify: fmt, spdlog, abseil — no regressions
- [ ] Fixture: `fixtures/sf7/pass_using_inside_function/` — `using namespace` inside a function → pass
- [ ] Fixture: `fixtures/sf7/fail_using_global/` — existing, stays fail
- [ ] Unit test for the new pattern

## Done

- Implemented together with #038 in a single commit `71e4fa3`
- `updateBlockCommentState()` helper + `braceDepth` in `scanFile`: two loop passes over the line (`{` before the check, `}` after) — handles inline lambdas correctly
- 5 new unit tests: function body, inline lambda, after closing brace, block comment, after block comment
- Fixtures: `fixtures/sf7_using_namespace/pass_using_inside_function/`, `pass_using_in_block_comment/`

## Changed files

| File | Change |
|------|--------|
| `src/rules/sf7_using_namespace.cpp` | `updateBlockCommentState()` + `braceDepth` + `inBlockComment` in `scanFile` |
| `tests/unit/rules/sf7_using_namespace_test.cpp` | 5 new tests |
| `fixtures/sf7_using_namespace/pass_using_inside_function/a.h` | new |
| `fixtures/sf7_using_namespace/pass_using_in_block_comment/a.h` | new |
