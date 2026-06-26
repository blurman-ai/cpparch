# [DOCS][SPEC] Separate shipped and future in architecture-spec

**Created:** 2026-06-23
**Started:** 2026-06-23
**Status:** completed
**Module:** DOCS / SPEC
**Priority:** major
**Complexity:** M
**Blocks:** trust in `docs/architecture-spec.md` as the main product/architecture document
**Blocked by:** —
**Related:** #006 (spec_refactor), #045 (docs_sync_roadmap_mvp_spec), #051 (config_loader_v1), #139 (core_gate_advisory_positioning), future/#005 (sarif_reporter_spec), future/#042 (clang_semantic_backend), future/#050 (sf21_anonymous_namespace)

> **Order:** comes AFTER #139. The layer taxonomy (core gate / structural advisory / hygiene advisory / history analytics) is defined by #139; this task only applies it to the spec and separates shipped/future. Do not redefine the layers here — otherwise the spec and README will diverge in terminology.

## Goal

Make `docs/architecture-spec.md` an honest document: shipped behavior is described as shipped, and future flags/modes/rules are not disguised as the current CLI contract.

## Context

Right now `architecture-spec.md` simultaneously plays the role of product spec, roadmap, and quasi-reference, which has accumulated false current promises inside it.

- The TL;DR and the `What it does` section present YAML module rules as the core of the product, even though `docs/MVP.md`
  and `--help` say plainly: in v0.1 they are only parsed and validated, not enforced:
  [docs/architecture-spec.md#L32-L40](../../docs/architecture-spec.md#L32-L40),
  [docs/architecture-spec.md#L136-L150](../../docs/architecture-spec.md#L136-L150),
  [docs/MVP.md#L33-L36](../../docs/MVP.md#L33-L36),
  [src/main.cpp#L25-L29](../../src/main.cpp#L25-L29).
- The `Commands` section shows flags and commands that don't exist in the binary:
  `--with-clang`, `--compile-commands`, `--format sarif`, `--metrics`:
  [docs/architecture-spec.md#L455-L479](../../docs/architecture-spec.md#L455-L479).
- The `Rule management` section promises `--disable=...` and inline suppression comments,
  but there are no such contracts in the code:
  [docs/architecture-spec.md#L237-L242](../../docs/architecture-spec.md#L237-L242).
- The technology stack in the spec still says "ryml or yaml-cpp", "Catch2 or GoogleTest",
  even though the shipped repo has long been fixed on `ryml` + `Catch2`:
  [docs/architecture-spec.md#L510-L517](../../docs/architecture-spec.md#L510-L517),
  [CMakeLists.txt#L46-L64](../../CMakeLists.txt#L46-L64).

The problem is not the future scope itself, but that the boundary between `is` and `will be` is blurred.

## Execution plan

- [ ] Separate, in the spec, the product current-state and roadmap examples: keep current commands in the reference, move future ones into v0.2+/appendix.
- [ ] Rewrite the TL;DR and `What it does` so that v0.1 is described as zero-config intrinsic rules + validated config surface, not as an enforced YAML architecture DSL.
- [ ] Explicitly mark flags/formats not supported today (`--with-clang`, SARIF, `--metrics`, suppressions) as future-only.
- [ ] Synchronize the tech stack and terminology in the spec with the real repo (`ryml`, `Catch2`, flat `src/rules/`, the `rule_set.cpp` factory).
- [ ] Re-read the `Stability contract`: does it promise interfaces that physically don't exist yet.

## Done

- `docs/architecture-spec.md` raised to v2.4.
- The TL;DR and `What it does` describe shipped v0.1 as a zero-config physical design gate + advisory regression layer.
- YAML module-rule enforcement, libclang, SARIF, `--metrics`, `--output`, rule toggles and suppressions are explicitly marked future-only.
- The CLI reference in the spec is reconciled with the actual `--help`.
- Tech stack synchronized with the repository: `ryml`, Catch2, CMake/Ninja, fast preprocessor backend.
- Roadmap reordered: v0.2 contains runtime enforcement of YAML module rules and the semantic backend.

## In progress

- (empty)

## Next steps

- None. The next spec update should start with checking `archcheck --help` and `CHANGELOG.md`.

## Key decisions

| Decision | Reason |
|----------|--------|
| Don't strip out the future scope, but mark it firmly | the roadmap is useful; what's harmful is exactly the unclear boundary between `now` and `later` |
| Reconcile commands only against the real `--help` | the spec should not invent a current CLI on top of the binary |
| Edit both the product sections and the technology section | otherwise a hidden conflict remains: "what we promise" vs "what it's actually built on" |

## Changed files

| File | Change |
|------|--------|
| `docs/architecture-spec.md` | separate shipped/future, rewrite commands and product sections |
| `docs/MVP.md` | if needed, clarify references to the spec/roadmap |
| `README.md` | if a new external desync surfaces after editing the spec |
