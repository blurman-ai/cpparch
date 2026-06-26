# [CONFIG][V1] Minimal config contract for the post-MVP phase

**Date created:** 2026-05-28
**Date started:** 2026-05-29
**Date completed:** 2026-05-29
**Status:** done
**Module:** CONFIG
**Priority:** major
**Difficulty:** S (spec and examples, no implementation)
**Target release:** v1 phase 1 (post-MVP)
**Blocks:** implementation of the config loader after MVP
**Blocked by:** —
**Related:** docs/architecture-spec.md, docs/MVP.md, future/v1_maj_agent_config_authoring_rules.md

## Goal

Lock the minimal `.archcheck.yml` format — only `version`, `modules`, `rules`.
No pattern rules, no per-rule severity, no inheritance, collectors, tags, policy engine.
A document is needed before the code, otherwise the format will sprawl across the README, the specs and the loader.

## Target shape

```yaml
version: 1

modules:
  domain:
    paths: ["src/domain/**"]   # glob only, no regex
  app:
    paths: ["src/app/**"]
  infra:
    paths: ["src/infra/**"]

rules:
  # Layered architecture: layers[0] may see everything below, layers[-1] sees no one above
  - type: layers
    name: main-layering
    layers: [app, domain, infra]   # highest → lowest (Lakos levelization)

  # Targeted prohibition (escape hatch or refinement on top of layers)
  - type: forbidden
    name: domain-no-infra
    from: [domain]
    to: [infra]

  # Mutual independence: modules of the same level must not know about each other
  - type: independence
    name: parallel-modules-isolated
    modules: [domain, infra]
```

**What is v1 phase 2, not phase 1:** `ignore`, `baseline`, `required`, fan-in threshold,
`auto_modules` (pattern-based slice discovery), `protected` type, per-rule `severity`.

## Design decisions

| Decision | Reason |
|---------|---------|
| `layers` as the main type | One contract replaces N×(N-1)/2 `forbidden` pairs; directly embodies Lakos levelization |
| `independence` as a separate type | Horizontal independence (one level) is needed separately from the vertical hierarchy |
| glob paths, no regex | Regex in module membership is too powerful and unpredictable for v1 |
| allowlist vs forbidden → **mixed by rule type** (2026-05-29) | `layers` / `independence` = implicit allowlist (strictness where there's a contract); `forbidden` = explicit blocklist (escape hatch for legacy and surgical override). No need to choose globally — the user mixes per-rule. Import Linter model. |
| Stale suppressions alerting (v1 phase 2) | Import Linter: `unmatched_ignore_imports_alerting` — if a suppress is stale, that too is a violation |
| `name` mandatory | Used in the violation output as a machine-readable id (`[rule:<name>]`) |

## References (read, links live as of 2026-05-28)

| Tool | What to take | Link |
|------------|-----------|--------|
| Deptrac | Shape: modules → ruleset; skip_violations as baseline | https://deptrac.github.io/deptrac/configuration/ |
| Import Linter | Typed contracts; `layers`, `independence`, `forbidden`, `protected` types; `ignore_imports` with wildcards | https://import-linter.readthedocs.io/en/stable/contract_types/ |
| dependency-cruiser | Rule buckets `forbidden`/`allowed`/`required`; `numberOfDependentsMoreThan` (god-headers); `moreUnstable` (Martin SDP); group matching $1 | https://github.com/sverweij/dependency-cruiser/blob/main/doc/rules-reference.md |
| ArchUnit | `slices().matching("pkg.(*)..")` → idea for auto_modules (v1 phase 2) | https://www.archunit.org/userguide/html/000_Index.html |
| Nx | tags as a secondary layer on top of path-based modules (future) | https://nx.dev/docs/features/enforce-module-boundaries |
| Bazel | `package_group` — named module groups for reuse in rules | https://bazel.build/concepts/visibility |

## Execution plan

- [x] Write `docs/config_format.md`: YAML schema, fields, rule types, examples
- [x] Lock the answer to the allowlist vs forbidden model question
- [x] 3-4 reference examples: tiny (2 modules), layered (3+ layers), legacy (forbidden only), mixed `include/`+`src/`
- [x] Explicitly spell out what is in scope of v1 phase 1 and what is not (table)
- [x] Describe backwards compatibility: `version: 1` — deliberate SemVer for the schema
- [x] Sync `docs/architecture-spec.md` §"Config-based analysis" (old format → pointer to the new doc)
- [x] Create an implementation task for the `src/config/` loader — `backlog/new/051_maj_config_loader_v1.md`

## Done

- 2026-05-29: wrote `docs/config_format.md` — single source of truth for `.archcheck.yml` v1.
  - Three top-level keys: `version` / `modules` / `rules`.
  - Three rule types: `layers`, `independence`, `forbidden` — semantics + the full fields of each.
  - Four reference examples: tiny (2 modules) / layered (3 layers) / legacy (forbidden only) / mixed (`include/` + `src/` + `layers` + `independence` + `forbidden` in one config).
  - Table "what is in scope of phase 1 and what is not" (`defaults`, `thresholds`, `baseline`, `ignore`, `required`, `protected`, `severity`, `auto_modules`, Nx tags, Bazel `package_group`, pattern rules — all deferred or dropped).
  - SemVer schema contract: MINOR/MAJOR table, schema version independent of the binary version.
  - Diagnostics contract: `[rule:<name>]` in text/JSON output — a stable machine-readable id, renaming = breaking change for baseline.
- 2026-05-29: locked the answer to allowlist vs forbidden — **mixed by rule type** (Import Linter model):
  - `layers` / `independence` — implicit allowlist (strictness where a contract is needed).
  - `forbidden` — explicit blocklist (escape hatch for legacy and surgical override).
  - No global choice needed: the user mixes per-rule in one config.
- 2026-05-29: `docs/architecture-spec.md` §"Config-based analysis" shortened: the old wall of text (`module: X, forbidden_deps`, pattern rules, `defaults`, `thresholds` in one chunk) removed; a short minimal example + a link to `docs/config_format.md` as the single source of truth left in place.

## Open questions / follow-up

- **Next task:** [`backlog/new/051_maj_config_loader_v1.md`](../new/051_maj_config_loader_v1.md) — implementation of the `src/config/` loader (YAML → `Config` struct, validation per `docs/config_format.md`, line-numbered errors, fixtures: 4 pass + 9 fail).
- **README.md config example** to be synced **after** the loader confirms the schema on a real repo — not now, otherwise there's a risk of drifting from the actual behavior before the first end-to-end run.
- **v1 phase 2** (`defaults`, `thresholds`, `baseline`, `ignore`) — a separate spec, after phase 1 loads via the loader and passes fixtures.
- **Open questions inside phase 1**: none left at closure — all design forks resolved in `docs/config_format.md`.

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| docs/config_format.md | new specification of the minimal format (created) | `4a14717` |
| docs/architecture-spec.md | §"Config-based analysis": old format replaced by a short example + pointer | `4a14717` |

## How it works

The contract is split into two layers:

1. **`docs/config_format.md`** — the authoritative source of the format. Describes three top-level keys (`version` / `modules` / `rules`), three rule types (`layers` / `independence` / `forbidden`), their semantics, the diagnostic format (`[rule:<name>]`) and the SemVer schema contract. It has four reference examples against which the loader (#051) is validated by fixtures.
2. **`docs/architecture-spec.md` §"Config-based analysis"** — a short overview with a pointer to (1). Doesn't duplicate the format, doesn't drift over time.

The key idea is **typed contracts**: the rule type determines the semantics (`layers`/`independence` — implicit allowlist, `forbidden` — explicit blocklist), the user mixes per-rule in one config. No global "allowlist project vs blocklist project" choice is needed. The model is copied from Import Linter.

`name` is mandatory on every rule — it's the machine-readable id in diagnostics (`[rule:<name>]`), and renaming = breaking change for baseline (this is deliberate).

## What governs it

- **Authority:** `docs/config_format.md` — single source of truth. Any other document (spec, README, AI agent prompt) points here, doesn't duplicate the format.
- **Evolution:** SemVer within the schema. Adding a top-level key with a default value — MINOR (still `version: 1`). Removing a key / changing semantics — MAJOR (`version: 2`). The archcheck binary reads any `version: 1` for its whole lifetime.
- **Phase 2 extension:** `defaults`, `thresholds`, `baseline`, `ignore` — added as new keys, without breaking phase 1 configs.

## What it's connected to

- **Produces:** the contract against which the loader is written → [`#051`](../new/051_maj_config_loader_v1.md).
- **Unblocks:** [`v1_maj_agent_config_authoring_rules.md`](../future/v1_maj_agent_config_authoring_rules.md) — without the format the AI agent had no target shape for the `.draft`. This dependency is lifted **after** #051 (the agent needs a working loader to validate its output).
- **Supersedes:** §"Config-based analysis" in `architecture-spec.md` v2.1 (the old format `module: X, forbidden_deps: [Y]` + pattern rules + `defaults` + `thresholds` in one wall of text). The old section is now a pointer.
- **Adjacent to:** [`#010`](../future/010_maj_ai_rule_synthesis_contract.md) — the old general synthesize contract (CLI shape, heuristic vs wrapper-prompt). #010 is broader (about CLI and modes), this task is about the YAML format itself that synthesize must produce.

## Diagnostics

If this task has "drifted" from reality, the symptoms and where to look:

- **The config loader (#051) validates something other than the spec** — fix `docs/config_format.md` (that's the contract), then sync the loader. Not the other way around.
- **The README shows the old format** — it's deliberately not synced, README sync awaits an end-to-end run through the loader (see follow-up above).
- **The AI agent writes config in an arbitrary shape** — that means [`v1_maj_agent_config_authoring_rules.md`](../future/v1_maj_agent_config_authoring_rules.md) is not yet implemented, the agent doesn't know about the targeted format. This is normal until #051 is closed.
- **Someone proposes adding `defaults`/`severity`/pattern rules to phase 1** — refuse, that's phase 2 or dropped, reasons locked in the "What is **not** in v1 phase 1" table in `docs/config_format.md`.
- **Conflict between `architecture-spec` §"Config-based analysis" and `docs/config_format.md`** — `config_format.md` wins. The spec is an overview document.
