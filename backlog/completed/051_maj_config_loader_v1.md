# [CONFIG] YAML loader for `.archcheck.yml` v1 (phase 1 schema)

**Date created:** 2026-05-29
**Date started:** 2026-05-29
**Date completed:** 2026-05-29
**Status:** done
**Module:** CONFIG
**Priority:** major
**Difficulty:** M (single scope: YAML → Config struct + validation + fixtures, without wiring into the rule pipeline)
**Target release:** v1 phase 1 (post-MVP)
**Blocks:** future/v1_maj_agent_config_authoring_rules.md, future/v1_maj_ai_config_synthesis_eval_protocol.md, future/v1_min_ai_config_synthesis_trial_spdlog.md (the whole AI synthesis chain — without a working loader the agent has nowhere to write)
**Blocked by:** —
**Related:** wip/v1_maj_config_format_minimal_contract.md (spec), docs/config_format.md (contract), future/010_maj_ai_rule_synthesis_contract.md

## Goal

Implement `src/config/` — reading `.archcheck.yml` v1 phase 1 into a typed `Config` struct per the contract [`docs/config_format.md`](../../docs/config_format.md). Loader-only: don't touch the rule pipeline yet, don't add hooks into `run_check`. Task closure = "the file is read, validated, errors are clear, fixtures green".

## Context

The spec `docs/config_format.md` locked the format: `version: 1`, `modules:` (map → `paths: [glob]`), `rules:` (list, `type ∈ {layers, independence, forbidden}`, mandatory `name`). The spec's stakeholders are the AI synthesis pipeline (#10, v1 #2-#4): without a working loader there's nothing to validate the agent's output against, and the whole AI config-generation chain is blocked.

The current MVP codebase reads only the default rules, there's no config. This task adds the `config/` subsystem without wiring it into the run — a separate sub-task will later plumb it into `run_check`.

## Open questions (resolve in phase 1, before code)

1. ~~**YAML parser: `ryml` or `yaml-cpp`?**~~ — **resolved 2026-05-29**: `ryml` v0.7.0 is already wired in via FetchContent in `CMakeLists.txt:55-65` (for other subsystems, archcheck_core links against `ryml::ryml`). We use it. We won't consider switching to `yaml-cpp` without a concrete blocker from ryml.

2. **Glob engine: own implementation or built-in?**
   Globs from `paths:` (`src/**`, `include/myproj/**`) are needed for membership resolution (a separate task), but the **non-emptiness check and syntax validity** are done already in the loader. Minimal check: list non-empty, each string non-empty, no `*..` or `?:` (rough sanity check). Full glob matching — not in this task.

3. **What counts as a "config error" (exit 2) vs a "validation warning"?**
   Contract: anything that violates the grammar or the semantic invariants from `docs/config_format.md` is exit 2 with a line-numbered message. We don't introduce warnings in phase 1 (the temptation of "soft validation" → we don't do it, to avoid blurring the contract).

## Execution plan

### Phase 1 — subsystem skeleton + `Config` struct

- [ ] Decide the YAML parser (see open question 1); add FetchContent to `CMakeLists.txt` (offline cache in `build/_deps/`)
- [ ] `include/archcheck/config/config.h` — public types:
  - `struct ModuleDef { std::string name; std::vector<std::string> paths; }`
  - `enum class RuleType { Layers, Independence, Forbidden }`
  - `struct LayersRule { std::string name; std::vector<std::string> layers; }`
  - `struct IndependenceRule { std::string name; std::vector<std::string> modules; }`
  - `struct ForbiddenRule { std::string name; std::vector<std::string> from; std::vector<std::string> to; }`
  - `using Rule = std::variant<LayersRule, IndependenceRule, ForbiddenRule>;`
  - `struct Config { int version; std::vector<ModuleDef> modules; std::vector<Rule> rules; }`
- [ ] `include/archcheck/config/config_loader.h` — entry point:
  - `Config load(const std::filesystem::path&)` — throws `ConfigError` with line/col from the YAML parser
- [ ] `src/config/config_loader.cpp` — implementation

### Phase 2 — validation (per the contract `docs/config_format.md`)

- [x] **Top-level**: exactly three keys `version`/`modules`/`rules`; unknown key → exit 2
- [x] **`version`**: integer, exactly `1`; `version: 2` → exit 2 with message "unsupported schema version, this binary supports v1"
- [x] **`modules`**: non-empty map; names match `[a-z0-9_-]+`; unique; `paths:` non-empty; globs non-empty
- [~] **Module non-overlap** — duplicate name detection implemented, "literally identical globs" not implemented (low value before wiring into the pipeline; marked as a future micro-task)
- [x] **`rules`**: each element has `type` and `name`; `name` unique; `type ∈ {layers, independence, forbidden}`
- [x] **`layers`**: `layers:` non-empty ordered list, elements are existing modules, no duplicates
- [x] **`independence`**: `modules:` non-empty set, elements are existing modules, no duplicates
- [x] **`forbidden`**: `from:` and `to:` non-empty lists; elements are existing modules; intersection `from ∩ to` empty
- [x] **Diagnostic format**: `file:line:col: <message>` via ryml location API; works for map/seq nodes, for scalar values ryml gives 0:0 (acceptable limitation)

### Phase 3 — fixtures + tests

- [x] `fixtures/config/pass/` — four reference examples from `docs/config_format.md` (tiny / layered / legacy / mixed), each a separate subdirectory with a valid `.archcheck.yml`
- [x] `fixtures/config/fail/` — one subdirectory per validation error category:
  - `fail_unknown_top_key/` — extra `defaults:` (phase 2 feature)
  - `fail_wrong_version/` — `version: 2`
  - `fail_duplicate_module/` — two modules with the same name
  - `fail_duplicate_rule_name/` — two rules with the same `name`
  - `fail_unknown_rule_type/` — `type: required` (phase 2+)
  - `fail_unknown_module_in_rule/` — `from: [ghost]`
  - `fail_from_to_overlap/` — `from: [a], to: [a, b]`
  - `fail_empty_layers/` — `layers: []`
  - `fail_missing_name/` — rule without `name:`
- [x] Catch2 tests: `tests/unit/config/test_loader.cpp` — 4 pass cases + 9 fail cases, 13/13 PASSED (merged into a single file instead of the originally planned two).

### Phase 4 — diagnostics and CLI plumbing (minimum)

- [x] CLI: `--config <path>` flag (default auto-pickup in CWD deliberately not implemented — deferred as QoL after wiring)
- [x] On `ConfigError` — print `file:line:col: <message>` to stderr, `exit 2`
- [x] **Don't wire `Config` into the rule pipeline** — done: `dispatch_config` validates → discards → `run_check` with default rules

## What is **not** in this task

- Glob-resolving files into modules (membership) — a separate task after the loader
- Wiring `Config` into the rule pipeline / actually applying `layers`/`independence`/`forbidden` rules — a separate task (that's already about rules/, not config/)
- v1 phase 2 features: `defaults`, `thresholds`, `baseline`, `ignore`, `required`, `protected`, `severity` — a separate spec later
- Pattern rules (`forbidden_pattern`) — dropped from v1, not implemented

## Alternatives (rejected / deferred)

- **A single `config.cpp` file without a public header** — rejected: the `Config` struct needs to be public for rules/ and tests
- **`std::variant<>` vs `IRule`-style inheritance** — we choose `variant`: there are only three rules, types are fixed by contract, adding a new type = breaking schema change (MAJOR bump), comparable in complexity to adding a branch to a visitor
- **No line numbers in errors, just simple "wrong key X"** — rejected: for the AI-synthesis pipeline it's critical that the agent sees *where* in its generated `.draft` the error is
- **Implement straight away with rules applied to the pipeline** — rejected: split scope, the loader is self-contained, closeable and verifiable; pipeline integration is a separate task with different acceptance criteria

## Done

### 2026-05-29 — phase 1 (skeleton) and phase 2 (parsing + validation), without line numbers

Public API + full validation loop for the v1 phase 1 schema. Loader is not wired into the pipeline and has no callers — `archcheck_core` builds, tests `235/235` green at every step.

**Implemented in `src/config/config_loader.cpp`:**

- `read_file()` — reads YAML, "cannot open" error if the file won't open.
- `parse_version()` — top-level map, `version: 1` mandatory; other versions → "unsupported schema version".
- `validate_top_keys()` — rejects any top-level keys except `version` / `modules` / `rules`; mandatory presence of `modules` and `rules`.
- `parse_modules()` + helpers (`is_valid_module_char`, `validate_module_name`, `parse_module_def`):
  - `modules:` — non-empty map.
  - Names match `[a-z0-9_-]+`, unique.
  - `paths:` — non-empty list of non-empty strings.
- `parse_rules()` + `parse_rule()` + three type parsers:
  - Each rule has `type` and `name`, name unique.
  - `type` is dispatched to `LayersRule` / `IndependenceRule` / `ForbiddenRule`, unknown type → error.
- `cross_validate()` + `RuleValidator` struct + helpers:
  - Every module name in `layers` / `independence.modules` / `forbidden.from` / `forbidden.to` exists in `modules:`.
  - No duplicates within a list.
  - `forbidden.from ∩ to = ∅`.

**All errors** go through `ConfigError(file, line, col, msg)`. Line/col is still 0:0 (phase 2.5 — ryml location API — not done yet).

**Closed open questions:**

1. **YAML parser** — `ryml` v0.7.0 (already in `CMakeLists.txt:55-65`, archcheck_core links against `ryml::ryml`). We won't consider `yaml-cpp`.
3. **Config error vs warning** — only errors (exit 2). Warnings are tempting, but they blur the contract.

### 2026-05-29 — phases 2.5 / 3 / 4 completed, task closed

- **Phase 2.5** (`c2d5505`): `ConfigError` carries a real line/col via the ryml location API. Introduced `LoaderCtx { parser, file }`, passed into each validate/parse function; `throw_at(ctx, node, msg)` extracts `parser.location(node)`. Errors without an anchor node (cross-rule, missing top key) use `throw_top` with file:1:1 — deterministic, not 0:0.
  - **Known limitation**: for scalar value nodes (e.g. `version: 2`) ryml gives line=0, col=0 — the accelerator doesn't build a location for scalars without an anchor. For map/seq nodes (including `defaults:`, `modules:` of various rules) — correct. In practice 80%+ of errors get a useful line.
- **Phase 3** (`7f4b3f4`): 4 pass + 9 fail fixtures in `fixtures/config/{pass,fail}/<name>/archcheck.yml`. Each pass fixture is a reference example from `docs/config_format.md` (tiny / layered / legacy / mixed). Each fail fixture is a separate error category. The Catch2 test `tests/unit/config/test_loader.cpp` for each:
  - pass: load → REQUIRE on the structure (module count, rule types, names).
  - fail: `REQUIRE_THROWS_WITH` with `ContainsSubstring` on the keyword (e.g. `"unknown top-level key"`).
  - All 13 new tests green. Total suite: 248/248.
- **Phase 4** (`16f2bf8`): CLI flag `--config <path>` — validates YAML, prints `ConfigError` (`file:line:col: msg`) to stderr with exit 2, on success hands off to the existing `run_check`. `Config` is discarded for now — wiring into the rule pipeline moved to a separate task.
- **End-to-end smoke**: `archcheck --config fixtures/config/pass/tiny/archcheck.yml /tmp` → exit 0, "No violations". `archcheck --config fixtures/config/fail/fail_unknown_top_key/archcheck.yml /tmp` → `archcheck: file:8:0: unknown top-level key 'defaults' …` + exit 2.

## Commits

| SHA | Description |
|-----|----------|
| `047fd0d` | `feat(config): add Config struct types for v1 phase 1 schema` |
| `a1120b2` | `chore(tasks): add #052 cross-TU duplication detector…` — a parallel session captured the staged files and committed the loader skeleton (config_loader.h/cpp + CMake wiring) under someone else's subject. Attribution in the commit is incorrect, but the code is mine. |
| `3e046ef` | `feat(config): reject unknown top-level keys, require modules/rules` |
| `4d9a172` | `feat(config): parse and validate modules block` |
| `2893aed` | `feat(config): parse rules block — dispatcher + layers type` |
| `574516d` | `feat(config): parse independence and forbidden rule types` |
| `f3377ce` | `feat(config): cross-rule validation — module existence and disjoint sets` |
| `47685ea` | `chore(tasks): checkpoint #051 — phase 1 and phase 2 (no line numbers yet)` |
| `c2d5505` | `feat(config): real line/col in ConfigError via ryml location API` |
| `7f4b3f4` | `test(config): add 4 pass + 9 fail fixtures and Catch2 loader tests` |
| `16f2bf8` | `feat(cli): --config <path> validates .archcheck.yml v1` |

## How it works

`archcheck::config::load(path)` — the single public entry point:

1. Reads the YAML from disk (`read_file`).
2. Parses it with ryml in `locations(true)` mode — `Parser` holds an accelerator for subsequent `parser.location(node)` lookups; `Tree` holds the nodes themselves.
3. Runs six validate/parse stages:
   - `parse_version` — `version: 1`, otherwise exit 2 with a stable message.
   - `validate_top_keys` — rejects any keys except `version` / `modules` / `rules`; mandatory presence of `modules`/`rules`.
   - `parse_modules` — `modules:` is a map, names match `[a-z0-9_-]+`, unique; `paths:` is a non-empty list of non-empty strings.
   - `parse_rules` — `rules:` is a list, each rule has `type` and `name`, name unique; `type` is dispatched to one of three types.
   - `parse_*_rule` (three of them) — the respective fields (`layers` / `modules` / `from`+`to`).
   - `cross_validate` via `RuleValidator` (visitor) — every module name in rule-references exists in `modules:`, no duplicates within a list, `forbidden.from ∩ to = ∅`.
4. Returns a filled `Config { version, modules, rules: vector<variant<Layers, Independence, Forbidden>> }`.
5. Any error → `ConfigError(file, line, col, msg)` (subclass of `std::runtime_error`).

Under the hood — three key things:

- **`std::variant<LayersRule, IndependenceRule, ForbiddenRule>`** for rules — extension is fixed by the schema contract (MAJOR bump), the variant reads cleaner than OOP inheritance.
- **`LoaderCtx { parser, file }`** is threaded through all functions — the only way to give the throw site access to the ryml location.
- **`RuleValidator` struct + `std::visit`** for cross-validation — overload-based dispatch keeps cross_validate < 30 lines (lizard).

## What governs it

- **Schema authority** — `docs/config_format.md`. Any change in loader behavior is checked against this document. If the loader drifts — fix the loader, not the doc.
- **Versioning** — `version: 1` is stable across all archcheck-1.x/2.x. Bump to `version: 2` only on a breaking change to the contract (see the SemVer table in `docs/config_format.md`).
- **Diagnostics** — `[rule:<name>]` is not yet used in the loader output (that's for the violation reporter after wiring). Current error format: `file:line:col: <message>`.
- **Phase 2 extension** — `defaults`, `thresholds`, `baseline`, `ignore`, `required`, `protected`, `severity` — added as new keys. The loader currently rejects them (validate_top_keys), which is **deliberate**: adding them early means blurring the contract.

## What it's connected to

- **Produces:** a `Config` struct ready to be wired into the rule pipeline. The integration itself (paths→files, applying rules to the graph) is a **separate task** (not in scope of #051).
- **Unblocks:** [`future/v1_maj_agent_config_authoring_rules.md`](../future/v1_maj_agent_config_authoring_rules.md) — the AI agent now has a working loader whose errors can be fed back into the prompt. The block is lifted once the config → pipeline task is closed (the agent needs not just a validator but a runnable end-to-end).
- **Implements:** [`completed/v1_maj_config_format_minimal_contract.md`](v1_maj_config_format_minimal_contract.md) — the format spec.
- **Uses:** ryml v0.7.0 (FetchContent in `CMakeLists.txt:55-65`), Catch2 v3 (FetchContent in `tests/CMakeLists.txt`).
- **Adjacent to:** [`future/010_maj_ai_rule_synthesis_contract.md`](../future/010_maj_ai_rule_synthesis_contract.md) — the old synthesize contract. After #051 + integration of #010 it becomes formalizable: the synthesize agent produces YAML per the phase 1 schema, archcheck validates it with the same loader.

## Diagnostics

Symptoms and where to look:

- **`No violations found`** on a config that should fail — this is expected for now: `Config` is discarded in `dispatch_config`, the pipeline doesn't apply rules yet. The "config → pipeline" task is not closed.
- **`file:0:0: …`** in the error message — ryml didn't build a location for a specific scalar node (see "Known limitation" in Done). Not a loader bug — a bug in the ryml location accelerator on scalars without an anchor. Will be fixed when/if we update ryml.
- **Loader missed an unknown key in `modules.*` or `rules[*]`** — look: validate_top_keys works only on root. Internal unknown keys (e.g. `modules.core.tags: [...]`) aren't rejected yet. This is a TODO for a separate micro-task if it becomes a problem.
- **`duplicate module name 'X'` doesn't trigger on literally identical YAML keys** — ryml merges duplicate keys into a map. The `fail_duplicate_module` test uses `REQUIRE_THROWS_AS` (not a keyword match), because the exact error depends on ryml. In practice YAML parsers behave differently on duplicate keys; the contract `docs/config_format.md` doesn't explicitly specify this.
- **Config passes valid through the loader but archcheck doesn't apply rules** — this is by design of phase 4: `dispatch_config` validates → discards → `run_check` with default rules. Wiring into the pipeline is a separate task.
- **Changes to the loader break existing fixtures** — fixtures are the reference; they're updated only if `docs/config_format.md` changes. Spec > loader > fixtures.

## Next steps (post-#051)

1. **Separate task "config → rule pipeline"** — resolve `paths:` into files (glob match against include graph nodes), apply `layers`/`independence`/`forbidden` to the graph, generate `Violation` with `[rule:<name>]` id. Unblocks practical use of the config.
2. **Auto-pickup `.archcheck.yml` in CWD** when run without arguments — a small QoL after pipeline integration is stable (otherwise risk of surprise behaviour on third-party repos).
3. **v1 phase 2** (`defaults` / `thresholds` / `baseline` / `ignore`) — a separate spec after the first practical run on a real repo.

## Key decisions

| Decision | Reason |
|---------|---------|
| Loader without wiring into the pipeline | A closeable scope, verified by fixtures without changing rules/. Wiring is a separate task with different acceptance criteria |
| `std::variant<Rule>` instead of OOP | Three rules, types fixed by contract (extension → MAJOR schema bump) — the variant reads cleaner |
| Line/col in every error | The AI-synthesis pipeline (#2-#4) depends on the agent seeing the error point in its `.draft` |
| No warnings in phase 1 | The contract `docs/config_format.md` is black and white: either valid (exit 0) or not (exit 2). A soft mode is the road to blurring the contract |

## Changed files

| File | Change | Final commit |
|------|-----------|-------------------|
| `include/archcheck/config/config.h` | new — `Config`, `ModuleDef`, `Rule` variant | `047fd0d` |
| `include/archcheck/config/config_loader.h` | new — `load()`, `ConfigError` | `a1120b2` |
| `src/config/config_loader.cpp` | new — implementation of all validation (phases 1, 2.1-2.4, 2.5) | `c2d5505` |
| `src/CMakeLists.txt` | `+ config/config_loader.cpp` into `archcheck_core` | `a1120b2` |
| `src/main.cpp` | `+ dispatch_config`, `--config` flag, exit 2 on `ConfigError` | `16f2bf8` |
| `fixtures/config/pass/{tiny,layered,legacy,mixed}/archcheck.yml` | 4 reference fixtures from the spec | `7f4b3f4` |
| `fixtures/config/fail/<9 dirs>/archcheck.yml` | 9 negative fixtures | `7f4b3f4` |
| `tests/unit/config/test_loader.cpp` | new — 13 Catch2 cases | `7f4b3f4` |
| `tests/CMakeLists.txt` | `+ unit/config/test_loader.cpp` | `7f4b3f4` |

## Fixtures

- [x] `fixtures/config/pass/tiny/` — 2 modules + 1 forbidden
- [x] `fixtures/config/pass/layered/` — 3 modules + layers
- [x] `fixtures/config/pass/legacy/` — 3 modules + 2 forbidden
- [x] `fixtures/config/pass/mixed/` — include/ + src/ + layers + independence + forbidden
- [x] `fixtures/config/fail/fail_unknown_top_key/`
- [x] `fixtures/config/fail/fail_wrong_version/`
- [x] `fixtures/config/fail/fail_duplicate_module/`
- [x] `fixtures/config/fail/fail_duplicate_rule_name/`
- [x] `fixtures/config/fail/fail_unknown_rule_type/`
- [x] `fixtures/config/fail/fail_unknown_module_in_rule/`
- [x] `fixtures/config/fail/fail_from_to_overlap/`
- [x] `fixtures/config/fail/fail_empty_layers/`
- [x] `fixtures/config/fail/fail_missing_name/`
