# archcheck config format v1 (phase 1)

Authoritative reference for `.archcheck.yml` schema starting at v1.

This document supersedes the YAML example in `docs/architecture-spec.md` §«Анализ по конфигу». Spec keeps a pointer here.

## Scope

Phase 1 of v1 ships **three required top-level keys** and **three rule types**, plus an optional `thresholds` block (landed early from phase 2). Everything else (`baseline`, `ignore`, `required`, `auto_modules`, `protected`, per-rule `severity`) is phase 2 or later — out of scope for this document.

The goal of the minimum contract is to be small enough that:

- a human can write a working `.archcheck.yml` in under five minutes after reading this doc;
- the loader implementation has no ambiguity to interpret;
- AI-generated drafts (see [v1_maj_agent_config_authoring_rules.md](../backlog/future/v1_maj_agent_config_authoring_rules.md)) have a fixed target shape.

## Top-level shape

```yaml
version: 1

modules:
  <module-name>:
    paths: [<glob>, ...]

rules:
  - type: layers | independence | forbidden
    name: <stable-id>
    # type-specific fields below
```

The three required top-level keys are `version`, `modules`, `rules`; `thresholds` is an optional fourth (see below). Any other key is a config error (exit code `2`).

### `version`

Integer. Currently `1`.

Schema versioning is independent of archcheck's product version: `version: 1` is the contract this document defines, and stays valid across all archcheck `1.x` and `2.x` releases until a MAJOR-breaking change to the schema bumps it to `version: 2`. See *Backwards compatibility* below.

### `modules`

A map from module name to module definition.

```yaml
modules:
  domain:
    paths: ["src/domain/**"]
  app:
    paths: ["src/app/**", "src/cli/**"]
```

- **Key** — module name. Lowercase, ASCII, `[a-z0-9_-]`. Used in `rules` references and in violation output. Must be unique.
- **`paths`** — non-empty list of globs (POSIX-style, `**` = any depth). A file belongs to a module iff at least one glob matches its repo-relative path. **No regex.** Regex in module membership is too powerful and too easy to get subtly wrong; if globs are not enough, the v1 answer is "split the module".

A file matched by globs of two different modules is a config error. Membership must be unambiguous.

Files not matched by any module are **unmoduled** — they are still scanned for default rules (SF.*, cycles, etc.) but cannot appear in `layers`/`independence`/`forbidden` rules. Unmoduled files are *not* an error; the config only constrains the parts you care about.

### `rules`

A list of rules. Order is irrelevant — rules are independent. Empty list is allowed (the config still gates default rules through future `defaults:` in phase 2).

Each rule has:

- **`type`** — one of `layers`, `independence`, `forbidden`. See below.
- **`name`** — stable string id. Used as the machine-readable rule id in violation output (`file:line: [rule:<name>] ...`). Must be unique across all rules. Recommended convention: kebab-case, scoped, e.g. `core-no-app`, `sinks-isolated`. Required because a numeric index is unusable in baselines and diffs.

The remaining fields depend on `type`.

### `thresholds`

Optional map of tunable rule defaults. Omit the block — or any individual key — to keep the embedded defaults; this is the file-override half of the model described under *Discovery & default resolution*.

```yaml
thresholds:
  chain_length: 10        # Lakos.ChainLength: max include-chain depth
  god_header_fan_in: 50   # Lakos.GodHeader: max fan-in before a header is a "god header"
```

- Each value must be a **positive integer**. Zero, negatives, and non-numeric values are a config error (exit code `2`).
- Unknown keys inside `thresholds` are a config error.
- Defaults when absent: `chain_length: 10`, `god_header_fan_in: 50`.

## Rule types

archcheck v1 uses **typed contracts** in the Import Linter sense, not a single bag of allow/forbid lists. The type carries the semantics:

| Type           | Semantics                                                          | Direction model       |
|----------------|--------------------------------------------------------------------|-----------------------|
| `layers`       | Strict Lakos levelization: upper layer may see lower, never up     | Implicit **allowlist** within the layered set |
| `independence` | Modules in the set must not know about each other (peer isolation) | Implicit **forbidden** between siblings       |
| `forbidden`    | Targeted "X must not depend on Y"                                  | Explicit **blocklist** (escape hatch / surgical) |

This is a deliberate mix — `layers` and `independence` give implicit-allowlist strictness where you want a contract, `forbidden` gives a no-tax escape hatch where you only want to nail one direction. You do not have to choose globally between "allowlist project" and "blocklist project"; you choose **per rule**, and a config can carry all three at once.

This resolves the open question from the originating task ([v1_maj_config_format_minimal_contract.md](../backlog/wip/v1_maj_config_format_minimal_contract.md) — "allowlist vs forbidden model"): **mixed by rule type**, modelled on Import Linter's typed contracts.

### `type: layers`

Lakos-style strict levelization across a list of modules.

```yaml
- type: layers
  name: main-layering
  layers: [app, domain, infra]    # highest to lowest
```

- **`layers`** — non-empty ordered list of module names. The first entry is the **highest** layer; the last is the **lowest**.
- A module in position `i` **may** depend on modules in positions `> i` (lower layers).
- A module in position `i` **must not** depend on any module in positions `<= i` *within this rule's list* — i.e. same-layer peers (itself excluded; self-deps inside a module are always fine) and any higher layer.

Equivalent to N×(N-1)/2 directional `forbidden` pairs, expressed in one declaration. This is the core reason `layers` exists: a four-layer hierarchy compresses six pairs into one rule, and the document still reads like an architecture.

Modules listed in `layers` must exist in `modules:`. Modules **not** listed are unconstrained by this rule (they may participate in other rules).

A config may carry multiple `layers` rules, e.g. one for `src/` and one for `tests/` if they have different hierarchies. Each rule is independent.

### `type: independence`

Horizontal peer isolation: modules listed in the set must not depend on each other in either direction.

```yaml
- type: independence
  name: feature-modules-isolated
  modules: [billing, search, notifications]
```

- **`modules`** — non-empty list of module names, set semantics (order irrelevant).
- For any two distinct `A`, `B` in the set: `A` must not depend on `B`, and `B` must not depend on `A`.
- Modules outside the set are unconstrained by this rule. In particular, `independence` says nothing about whether `billing` may depend on some `core` not in the list — only about peer relationships *inside* the set.

Use this when peer modules at the same logical level should not know about each other (typical for feature modules, plugin modules, parallel adapters). It is **not** a substitute for `layers` — it does not impose direction, only mutual ignorance.

### `type: forbidden`

Targeted directional ban.

```yaml
- type: forbidden
  name: core-no-app
  from: [core, details]
  to:   [app, ui]
```

- **`from`** — non-empty list of module names. The "subject" side: files in any of these must not depend on the target side.
- **`to`** — non-empty list of module names. The "object" side: files in any of these are off-limits for the subject side.
- For each `f` in `from` and each `t` in `to`: any include from a file in module `f` to a file in module `t` is a violation.
- `from` and `to` must be disjoint. A rule that bans a module from depending on itself is a config error — that is what `independence` is for, and even there self-deps are always fine.

`forbidden` is intentionally the simplest rule type. It exists for two cases:
- **Legacy adoption** — when no clean hierarchy exists yet, but a couple of inversions are known and worth gating.
- **Surgical override** — when `layers` mostly captures the architecture but a single extra inversion needs to be banned that doesn't fit cleanly into the layering (e.g. "infra may see domain, except for `infra/legacy_codec/` which must not").

If you find yourself writing more than ~5 `forbidden` rules for a single project, consider whether `layers` or `independence` would replace them.

## Reference examples

### 1. Tiny — two modules

Minimum viable config. Works for a small library with one boundary that matters.

```yaml
version: 1

modules:
  core:
    paths: ["src/core/**"]
  ui:
    paths: ["src/ui/**"]

rules:
  - type: forbidden
    name: core-no-ui
    from: [core]
    to:   [ui]
```

Reads as: "core must not include ui". Done.

### 2. Layered — three layers

Classic Lakos hierarchy. One rule expresses six forbidden directions.

```yaml
version: 1

modules:
  domain:
    paths: ["src/domain/**"]
  app:
    paths: ["src/app/**"]
  infra:
    paths: ["src/infra/**"]

rules:
  - type: layers
    name: main-layering
    layers: [app, domain, infra]
```

`app` may use `domain` and `infra`. `domain` may use `infra`. Nothing goes upward.

### 3. Legacy — forbidden only

A real project where no full hierarchy exists yet, but a few directions are known and worth pinning. Use `forbidden` exclusively until the architecture is clear enough for `layers`.

```yaml
version: 1

modules:
  ui:
    paths: ["src/ui/**"]
  storage:
    paths: ["src/storage/**"]
  net:
    paths: ["src/net/**"]

rules:
  - type: forbidden
    name: storage-no-ui
    from: [storage]
    to:   [ui]

  - type: forbidden
    name: net-no-ui
    from: [net]
    to:   [ui]
```

This is the entry point for legacy codebases — nothing is *required* to be allowed, only specific inversions are banned. New code in any direction not listed is fine, which is what makes the config adoptable on day one.

### 4. Mixed — `include/` + `src/`, layering + peer isolation

Realistic medium-sized project: public headers in `include/`, implementation in `src/`, with feature modules that must not cross-pollinate.

```yaml
version: 1

modules:
  public_api:
    paths: ["include/myproj/**"]
  core:
    paths: ["src/core/**"]
  billing:
    paths: ["src/features/billing/**"]
  search:
    paths: ["src/features/search/**"]
  infra:
    paths: ["src/infra/**"]

rules:
  - type: layers
    name: vertical-layering
    layers: [billing, search, public_api, core, infra]

  - type: independence
    name: features-isolated
    modules: [billing, search]

  - type: forbidden
    name: public-no-infra
    from: [public_api]
    to:   [infra]
```

`layers` establishes the vertical hierarchy. `independence` adds the horizontal constraint that `billing` and `search` (same logical level) must not know about each other — `layers` alone permits this because peers are unconstrained. `forbidden` adds the surgical rule that public headers must not leak `infra` types even though the layering would technically allow it.

## Discovery & default resolution

archcheck follows the **embedded-defaults + file-override** model used by clang-tidy, rustfmt, and clippy — the right fit for a single static binary that must be useful zero-config.

### Where defaults live

Every tunable default lives as a named field on the internal `Config` struct with an in-code initializer — a single source of truth. No magic numbers scattered across rule implementations. The shipped defaults are:

| Default | Value | Rule / subsystem |
|---------|-------|------------------|
| include chain length | `10` | `Lakos.ChainLength` |
| god-header fan-in | `50` | `Lakos.GodHeader` |
| project source extensions | `.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .tpp .inl .inc` | `scan` |
| header extensions | `.h .hh .hpp .hxx .ipp .tpp .inl .inc` | `scan` |

These are compiled into the binary; **no `.archcheck.yml` is required** for a useful run. The rule-mandated constants that are *not* defaults — `#pragma once` / include-guard syntax, `.inc` = single-include semantics, SF.7 brace-depth tracking — stay fixed in code and are intentionally not overridable.

### Where the user file is found

If `--config <path>` is given, that exact file is loaded and discovery is **skipped**.

Otherwise archcheck walks **up** from the current working directory to the filesystem root and loads the **first** `.archcheck.yml` it finds. First-found wins; the search stops there — it does not continue past it and does not merge a chain. This matches `.clang-format` / eslint behaviour and lets archcheck run from any subdirectory of a project.

If no file is found anywhere up the tree, the embedded defaults are used unchanged.

### How they combine

The discovered file is merged **over** the embedded defaults: the user writes only the keys they want to change; every unspecified key keeps its compiled-in default. A key present in the file replaces the corresponding default; a key absent is left untouched.

Threshold overrides (the `thresholds:` block, documented above) follow exactly this merge model — e.g. a file with only `thresholds: { god_header_fan_in: 30 }` changes that one value and leaves chain length at `10`.

## What is **not** in v1 phase 1

| Feature                              | Status      | Where it goes        |
|--------------------------------------|-------------|----------------------|
| `defaults:` block (toggle SF.*, etc.) | v1 phase 2 | Same file, new key   |
| `baseline:` / `--baseline` integration in YAML | v1 phase 2 | Same file, references existing baseline file |
| `ignore:` per rule (suppression lists) | v1 phase 2 | New per-rule key     |
| `required:` rule type ("A must depend on B") | v1 phase 2+ | New rule type    |
| `protected:` rule type (Import Linter)        | v1 phase 2+ | New rule type    |
| `severity:` per rule                          | v1 phase 2+ | New per-rule key |
| Per-rule `unmatched_ignore_imports_alerting`  | v1 phase 2+ | Stale-suppression detection (Import Linter idea) |
| `auto_modules:` / slice discovery patterns    | v1 phase 3 | ArchUnit-style `slices().matching("pkg.(*)..")` analogue |
| Tag-based modules (Nx)                        | v1 phase 3 | Secondary layer over path-based modules |
| `package_group` named groups (Bazel)          | v1 phase 3 | Reusable module sets across rules |
| Pattern-based content rules (`forbidden_pattern`) | dropped from v1 | regex content checks are out of archcheck's scope (linter territory) |
| Per-rule message override                     | dropped from v1 | message is derived from rule type and name |

Phase 2 is additive: any v1-phase-1 config remains valid in phase 2. Adding a top-level key with a default value is a MINOR change (see *Backwards compatibility*).

## Diagnostics contract

Every violation references the rule by `name`:

```
src/ui/widget.cpp:42:1: [rule:storage-no-ui] storage must not depend on ui
                                              (via #include "storage/backend.h")
```

- The bracketed `[rule:<name>]` is part of the stable text format and the JSON `rule_name` field.
- Rule type is **not** in the text format. The rule name is the contract; type is loader-side metadata.
- Configs without `name` on a rule fail to load with exit code `2`.

This makes baseline files, diff-mode output, and downstream tooling key off `name` — which means renaming a rule in the config is a breaking change for the project's baseline. That is intentional: rule renames should be deliberate.

## Backwards compatibility

`version: 1` is a SemVer contract over the schema itself, independent of the archcheck binary version.

| Schema change                                       | Bump        |
|-----------------------------------------------------|-------------|
| Adding a new top-level key with a default value     | MINOR (still `version: 1`) |
| Adding a new rule `type`                            | MINOR       |
| Adding an optional field to an existing rule type   | MINOR       |
| Removing a top-level key                            | MAJOR (`version: 2`) |
| Changing the meaning of an existing field           | MAJOR       |
| Removing a rule type                                | MAJOR       |
| Making a previously optional field required         | MAJOR       |

Phase 2 additions (`defaults`, `thresholds`, `ignore`, etc.) are all MINOR — `version: 1` stays.

archcheck reads any `version: 1` config across its full lifetime. A config with a future `version: 2` produces exit code `2` with a clear "unsupported schema version" message when read by an older binary.

## References

| Tool                    | What this format borrows                                         | Link |
|-------------------------|------------------------------------------------------------------|------|
| Import Linter (Python)  | Typed contracts (`layers`, `independence`, `forbidden`); rule `name` as id | <https://import-linter.readthedocs.io/en/stable/contract_types/> |
| Deptrac (PHP)           | `modules` → `paths` shape; baseline as separate file (phase 2)   | <https://deptrac.github.io/deptrac/configuration/> |
| dependency-cruiser (JS) | Mixed allow/forbid model; rule id used downstream                | <https://github.com/sverweij/dependency-cruiser/blob/main/doc/rules-reference.md> |
| ArchUnit (Java)         | Slice discovery (`auto_modules`, deferred to phase 3)            | <https://www.archunit.org/userguide/html/000_Index.html> |
| Lakos, *LSCD* / *LDPCC* | Levelization → `layers` semantics                                | — |

## Next steps

After this document is accepted:

1. File an implementation task for `src/config/` loader (read YAML → internal `Config` struct, validate, surface `version`/`modules`/`rules` errors with line numbers).
2. After loader lands, update README to point at a single reference example consistent with this doc.
3. v1 phase 2 starts as a separate task once a real user has exercised phase 1 end-to-end.
