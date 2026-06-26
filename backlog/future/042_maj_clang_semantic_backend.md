# [SCAN][RULES][BUILD] libclang semantic backend (--with-clang)

**Created:** 2026-05-29
**Start date:** —
**Status:** new
**Module:** SCAN / RULES / BUILD
**Priority:** major
**Complexity:** L (multi-day, splits into phases)
**Blocks:** SF.21, SF.2, SF.5, SF.10, SF.11, Martin Abstractness (all semantic rules v0.2+)
**Blocked by:** ~~#043 (spike_clang_perf)~~ — **unblocked 2026-05-29**, the spike confirmed the two-backend scheme ([`docs/dev/spike_clang_perf.md`](../../docs/dev/spike_clang_perf.md)): libclang ×1350 slower than regex, ~9 minutes on a 5000-TU monorepo single-thread. The scope of this task stays as written, `--with-clang` is preserved.
**Related:** #5 (gh — owner task), #6 (gh — Issue 6 audit), #006 (spec_refactor — laid down the two-backend scheme), #028 (rules_engine_mvp — deferred SF.21 "requires libclang"), #035 (sf7_brace_depth — marked precise parsing as libclang/v0.2), #008 (dependency_graph_foundation), #043 (spike_clang_perf — confirmed the architecture)

> **2026-05-29:** deferred to `future/` after closing #043. v0.2 material — a separate release narrative (semantic rules), does not block v0.1. Pick up when there is explicit user demand for SF.5/10/11/21 or once v0.1 ships.

## Goal

Set up an optional libclang backend under the flag `--with-clang`, giving rules access to the AST, and run the first semantic rule (SF.21) through it as a vertical slice.

## Context

The two-backend scheme is a decision settled in #006: fast (preprocessor scan) = default in v0.1, libclang = opt-in / fully fledged in v0.2. But the backend has no task owner — it lives only in `docs/architecture-spec.md` (§"Two-backend scheme", the planned `clang_scanner.h/.cpp`, the example `--with-clang --compile-commands`) and as "where it was deferred" in closed tasks. This task is the owner of the v0.2 phase.

What it unblocks: the semantic SF.2 / SF.5 / SF.10 / SF.11 / SF.21 and Martin's Abstractness — everything that cannot be reliably extracted by a preprocessor scan.

Two invariants that must not be broken:

1. **Zero-setup does not break.** The tool builds and all v0.1 rules work without LLVM installed. libclang — a guarded optional dependency. `--with-clang` without libclang in the build → a clean error, not a crash.
2. **Fast-backend rules do not depend on clang.** SF.7/8/9, Lakos.* must know nothing about AST types.

## Open question (resolve in phase 1)

**How does a semantic rule obtain the AST without polluting `IRule`?**

The current `IRule::check(graph, readFile)` = the include graph + a textual `readFile`. Add the AST to it — and all fast-backend rules start transitively depending on a clang type they don't use (an ISP violation).

The proposed direction (confirm/refute in phase 1): a separate `ISemanticRule` with `check(SemanticContext&)`, where `SemanticContext` is a thin wrapper over `clang::ASTContext` / the cursor API. `rule_set` runs the `ISemanticRule` set only on `--with-clang`. The fast set stays on `IRule` untouched. No shared base interface "just in case" — two sets, two passes, an explicit fork in `run_check`.

## Execution plan

Three vertical slices. Each — a candidate to spin off into 042a/b/c when moving into `wip/` (after the 008 → 008a–h pattern). Do not spawn files in advance.

### Phase 1 — 042a: backend skeleton + CMake opt-in + flag plumbing

- [ ] CMake: `option(ARCHCHECK_WITH_CLANG "..." OFF)`; `find_package(Clang CONFIG)` under a guard; on OFF/absent — `clang_scanner.cpp` is not compiled, the target builds without LLVM
- [ ] `include/archcheck/scan/clang_scanner.h` — the public shape (types + the TU parse signature), without logic
- [ ] `src/scan/clang_scanner.cpp` — a stub under `#ifdef ARCHCHECK_WITH_CLANG`
- [ ] CLI: the flags `--with-clang` and `--compile-commands <path>`; without libclang in the build `--with-clang` → exit with a clear "rebuild with `-DARCHCHECK_WITH_CLANG=ON`"
- [ ] Resolve the open question: set up `ISemanticRule` / `SemanticContext` (or justify a different choice) — record it in "Key decisions"
- [ ] Smoke test: a build with `-DARCHCHECK_WITH_CLANG=ON` is green; a build without the flag is green and does not pull in LLVM
- [ ] CI: add an optional matrix leg with libclang (do not break the main leg without LLVM)

### Phase 2 — 042b: loading compile_commands + first AST fact

- [ ] Loading `compile_commands.json` (clang `CompilationDatabase`); a clear error on absence
- [ ] Parse one TU into an AST via `SemanticContext`
- [ ] Extract one verifiable fact (e.g., the list of namespace declarations in the TU) — prove that AST data flows down to where the rules will live
- [ ] Fixtures: a minimal project with `compile_commands.json` (1–2 TU)
- [ ] Test: the fact is extracted deterministically

### Phase 3 — 042c: first semantic rule SF.21 (vertical slice)

- [ ] `include/archcheck/rules/sf21_anon_namespace_in_header.h` + `src/rules/...cpp` on `ISemanticRule`
- [ ] Rule: an anonymous namespace in a header file (by `isHeaderFile()`) → a violation with `file:line`
- [ ] Registration in the semantic set of `rule_set`, run only on `--with-clang`
- [ ] Fixtures: `fixtures/sf21/pass/` (anon ns only in .cpp), `fail_in_header/` (anon ns in .h)
- [ ] Test: pass = 0 violations, fail = 1 violation with the correct line
- [ ] Remove the "SF.21 → v0.2" mark from #028 (closed)

## Alternatives (rejected / deferred)

- **First rule SF.2 instead of SF.21** (use namespaces for logical structure) — rejected as the first slice: a fuzzy criterion, debatable defaulting, more about style than an unambiguous AST fact. SF.21 is binary, widely accepted (Core Guidelines), already marked as a libclang candidate in #028. SF.2 is set up as a separate task after the backend settles.
- **tree-sitter instead of libclang** — gives a syntax tree without semantics (name resolution, namespaces, ODR). Insufficient for SF.2/5/10/11. Recorded in `architecture-spec` §528.
- **Semantic edges into the shared `DependencyGraph`** (clang resolution refines the existing graph for SF.9/Lakos) — tempting, but that is separate value and a separate scope. Don't mix it with "AST access for new rules". A candidate for a separate v0.2+ task, not here. YAGNI.
- **A single base `IRule` with optional AST** — rejected: an ISP violation, the fast set must not depend on clang. Two sets, an explicit fork.

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. ~~Wait for the results of the #043 spike~~ — **done 2026-05-29.** The two-backend scheme is confirmed by measurement.
2. Start with phase 1 (CMake opt-in + the `clang_scanner.h/.cpp` skeleton + `--with-clang` + the `ISemanticRule` decision).
3. Account for in phase 1: libclang version **≥18** (the spike measured 19; 11 is outdated, not a target). Record it in the CMake `find_package` logic and in the CI matrix.

## Key decisions

| Decision | Reason |
|---------|---------|
| libclang — opt-in, not default | we don't break zero-setup, new users should not have to install LLVM |
| Separate `ISemanticRule` (prelim.) | ISP — the fast set must not transitively pull in clang types |
| First rule — SF.21, not SF.2 | a binary AST fact vs a fuzzy style |

## Changed files

| File | Change |
|------|-----------|
| `CMakeLists.txt` / `src/CMakeLists.txt` | `ARCHCHECK_WITH_CLANG` option, guarded `find_package(Clang)` |
| `include/archcheck/scan/clang_scanner.h` | new (phase 1) |
| `src/scan/clang_scanner.cpp` | new, under `#ifdef` (phases 1–2) |
| `include/archcheck/rules/i_semantic_rule.h` | new (phase 1, if `ISemanticRule` is confirmed) |
| `include/archcheck/rules/sf21_anon_namespace_in_header.h` + `.cpp` | new (phase 3) |
| `src/rules/rule_set.cpp` | the semantic set under `--with-clang` |
| CLI (entry point) | the flags `--with-clang`, `--compile-commands` |
| `fixtures/sf21/{pass,fail_in_header}/` | new (phase 3) |
| `tests/...` | clang scan + SF.21 |
| `.github/workflows/ci.yml` | an optional libclang matrix leg |

## Fixtures

- [ ] `fixtures/sf21/pass/` — anonymous namespace only in `.cpp`
- [ ] `fixtures/sf21/fail_in_header/` — anonymous namespace in `.h`
