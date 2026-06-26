# [RULES] SF.21 — anonymous namespace in `.h` (moved to v0.3)

**Date created:** 2026-05-29
**Date started:** —
**Status:** new
**Module:** RULES
**Priority:** minor
**Difficulty:** S (via libclang) / M (reliable text-scan)
**Target release:** **v0.3** (see rationale)
**Blocks:** —
**Blocked by:** #042 (clang_semantic_backend) — if done precisely via AST

> **2026-05-29:** stage 1 (docs sync) was done before the v0.1 release and committed; stage 2 (implementation) is deferred to `future/` together with #042 — both are handled in v0.2+. See `## Done` below.
**Related:** #028 (rules_engine_mvp, already deferred SF.21 with the note "requires libclang"), #006 (spec_refactor), #042 (clang_semantic_backend)

## What this is

The C++ Core Guidelines rule [SF.21](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rs-unnamed): "Don't use an unnamed (anonymous) namespace in a header".

```cpp
// foo.h — SF.21 VIOLATION
namespace {                // ← anonymous namespace at the top level of a header
  int counter = 0;
  void helper() { ... }
}
```

An anonymous namespace gives internal linkage. In a `.h` this means: every translation unit that includes the header gets **its own separate copy** of the objects and functions. Not a formal ODR violation, but useless binary bloat and source surprises (`&helper` differs across TUs, static variables are duplicated).

## Why moved from v0.1/v0.2 to v0.3

**1. This isn't about architectural drift.** Drift in archcheck is a change in the **shape of the dependency graph**: shortcut edges (DRIFT.1), growing SCCs (DRIFT.2), god-headers, lengthening chains, breaking module boundaries. An anonymous namespace in a header doesn't touch the graph — it's an ODR pitfall and binary bloat, micro-level hygiene of the same class as SF.7 (`using namespace` in a header). Conceptually closer to clang-tidy than to physical design in the Lakos sense.

**2. It's not the most common AI pattern.** A typical AI agent refactoring code is more likely to write a shortcut import (DRIFT.1), create a cycle when "extracting common code into a helper" (DRIFT.2), forget a guard in a new header (SF.8), or copy `using namespace` (SF.7). An anonymous namespace in a `.h` is a counter-intuitive pattern, more often written by juniors by hand, copied from a `.cpp` without understanding. An AI more often picks `static inline` or `inline` (which is correct) or moves it into a `.cpp`.

**3. The precise version isn't expensive, but it isn't critical either.** Via libclang the definition `clang::NamespaceDecl::isAnonymousNamespace()` is trivial — S. Dragging in a text-scan version for v0.1 just to tick a box in the MVP checklist is extra code with no product value. clang-tidy (`google-build-namespaces`) and cppcheck already cover this rule; the users who need it will get it anyway.

**4. v0.1 value is physical design + AI drift, not SF hygiene.** The v0.1 cover story in the spec: "module boundaries + cycles in CI". SF.7/8/9 are included because they're hygiene add-ons for physical design (`using namespace` breaks namespace boundaries; a missing guard breaks the include graph). SF.21 doesn't fit even that logic — an anonymous namespace in a `.h` breaks neither boundaries nor the graph.

**5. v0.3 fits better than v0.2.** v0.2 is the libclang backend + the remaining SF rules (SF.2/5/10/11) + SARIF. Those four SF rules are about correctness of the physical layer (defs in headers, .cpp→.h pairing, no implicit includes, self-contained). SF.21 is a separate hygiene task, it's an "accidental guest" there. v0.3 is the C/I/NL sections + BDE + the AI loop; SF.21 fits there logically: "the rest of the hygiene rules from CCG that aren't critical for physical design".

## Plan

### Stage 1 — document synchronization (can be done without #042)

- [x] Update ROADMAP.md: remove SF.21 from v0.1 blockers, clarify v0.2 (preview via `--with-clang`), add to v0.3 as default-ON.
- [x] Update architecture-spec.md: SF.* table + v0.3 section, paths to 050 (new → wip).
- [x] Check MVP.md, CHANGELOG.md, README.md — SF.21 mentions don't contradict the decision.

### Stage 2 — implementation (after #042 / clang_semantic_backend)

- [ ] Spec of the SF.21 rule: `Sf21NoAnonymousNamespace` in `src/rules/`, alongside the other SF classes. Uses libclang (#042) — `clang::NamespaceDecl::isAnonymous()` on translation units from `compile_commands.json`. Comes down to ~30 lines.
- [ ] Registration in `makeDefaultRuleSet` via `rules.push_back(std::make_unique<Sf21NoAnonymousNamespace>())`. OFF by default until v0.3 (enabled via `--with-clang` in v0.2 as a preview, if #042 has already shipped).
- [ ] Fixtures `fixtures/sf21_anonymous_namespace/pass/` (named namespace + .cpp with anon) and `fixtures/sf21_anonymous_namespace/fail/` (anon in `.h`).
- [ ] Unit test with a libclang mock or a live `compile_commands.json` from the fixture.
- [ ] CHANGELOG (under v0.3).

## Changed files (plan)

| File | Change |
|------|--------|
| `include/archcheck/rules/sf21_anonymous_namespace.h` | new |
| `src/rules/sf21_anonymous_namespace.cpp` | new |
| `src/rules/rule_set.cpp` | registration |
| `tests/unit/rules/sf21_anonymous_namespace_test.cpp` | new |
| `fixtures/sf21_anonymous_namespace/` | new (pass + fail) |
| `docs/architecture-spec.md` | SF.* table + roadmap v0.1/v0.2/v0.3 |
| `docs/MVP.md` | (if mentioned) |
| `docs/STATUS.md` | remove SF.21 from "open until v0.1" |

## Done

**2026-05-29 — stage 1: document synchronization**

- `docs/ROADMAP.md` — SF.21 removed from v0.1 blockers (decision recorded), clarified in v0.2 as a preview via `--with-clang`, item added to v0.3 (default-ON).
- `docs/architecture-spec.md` — SF.* table: phase changed from "v0.3" to "v0.2 (preview, `--with-clang`) / v0.3 default-ON"; paths to 050 moved `new → wip`; v0.3 section clarified.
- `docs/MVP.md`, `CHANGELOG.md` — SF.21 not mentioned, no edits needed.
- `README.md:111` — already correct (SF.21 unlocks with `--with-clang` in v0.2), not editing.

Stage 2 (rule implementation) waits on #042 (clang_semantic_backend).

## How it works

(to be filled in at closing)
