# archcheck

[![CI](https://github.com/blurman-ai/cpparch/actions/workflows/ci.yml/badge.svg)](https://github.com/blurman-ai/cpparch/actions/workflows/ci.yml)

Architecture testing and dependency rules for C++ projects.

## Why

C++ projects degrade over time:

- layers stop being layers
- modules start depending on everything
- include graph turns into spaghetti

Code review doesn’t catch this.
Linters don’t check architecture.
**AI-generated code accelerates the drift** — agents suffer *constraint decay* ([Dente et al., EURECOM, 2026](docs/research/constraint_decay.md)): ~30 pp drop in assertion pass rate when functional tasks gain structural constraints. The prompt degrades with context; CI doesn’t.

**archcheck enforces architectural rules in CI.**

---

## What it does (today, v0.1)

- Scans `.h` / `.cpp` sources with a fast preprocessor pass — no `compile_commands.json` required
- Builds the include-dependency graph and detects cycles, deep chains, and god-headers
- Runs five default intrinsic rules sourced from C++ Core Guidelines and Lakos:
  - **SF.7** — no `using namespace` in headers
  - **SF.8** — every header has `#pragma once` or include guard
  - **SF.9** — no cycles in the include graph
  - **Lakos.GodHeader** — fan-in ≤ 50 incoming includes
  - **Lakos.ChainLength** — include-chain depth ≤ 10
- Reports violations as `file:line: [rule] message`, exit non-zero on failure
- Tracks architectural drift between revisions: `--save-graph-baseline` once, then `--drift-baseline` in CI catches new cycles (DRIFT.2) and short-circuit edges (DRIFT.1), and reports new mutual module coupling (DRIFT.3, advisory)
- Diff mode (`--diff <revspec>`) compares the include graph between two git refs and reports structural regressions — added/removed edges, grown cycles, new god-headers, chain-length growth — useful for PR checks on legacy projects

---

## Quick start

```bash
# Default check on current directory — zero config, no flags
archcheck

# Check a specific path
archcheck path/to/src

# JSON output (for CI integration)
archcheck --format json src/

# Freeze legacy violations, fail only on new ones
archcheck --save-baseline archcheck-baseline.json src/
archcheck --baseline      archcheck-baseline.json src/

# Snapshot the include graph, then track drift in PRs
archcheck --save-graph-baseline graph.json src/
archcheck --drift-baseline      graph.json src/

# Report only what changed in a git range
archcheck --diff main..feature src/

# Validate .archcheck.yml and apply threshold overrides
archcheck --config .archcheck.yml src/

# Advisory duplication report (report-only, never gates)
archcheck --duplication src/

# Quick previews: scanner stats / include-graph stats
archcheck --scan src/
archcheck --graph src/
```

### Example output

```text
$ archcheck fixtures/sf9_no_cycles/fail_simple
a.h: [SF.9] cycle: a.h → c.h → b.h → a.h

1 violation(s) (SF.9: 1)
$ echo $?
1
```

---

## Key Features

- **Zero config** — runs with no arguments, ships sensible defaults; no `compile_commands.json` needed
- **CI-first** — non-zero exit on violations, deterministic output
- **Baseline-friendly** — freeze legacy, fail on new (`--baseline`, `--save-baseline`)
- **Drift detection** — track architecture across revisions (`--drift-baseline`, `--diff`)
- **Sourced** — every default rule cites a published authority (Core Guidelines, Lakos)

---

## Output formats

- `text` (default) — human-readable, line-oriented
- `json` — stable schema for CI integration

---

## What it is NOT

- Not a linter (clang-tidy's job)
- Not a bug finder (PVS-Studio, Coverity, cppcheck)
- Not a formatter (clang-format)
- Not an include optimizer (IWYU)
- Not a GUI, not a web dashboard, not an IDE extension — CLI and CI only

archcheck checks architecture only.

---

## Planned (v0.2+)

The original pitch promised module-mapping with YAML-declared dependency rules. This is real, just deferred — see [docs/architecture-spec.md](docs/architecture-spec.md) and the ADR trail in `backlog/completed/`.

- **YAML module-rule enforcement** — the `.archcheck.yml` v1 schema (`modules:` + `rules:` with `layers` / `independence` / `forbidden`, see [docs/config_format.md](docs/config_format.md)) is already parsed, validated and auto-discovered, and `thresholds:` overrides already apply; *enforcement* of module rules lands v0.2
- **`archcheck init`** scaffolding command — v0.2
- **`--with-clang`** semantic backend (libclang) — unlocks SF.21 and the remaining SF.* rules — v0.2
- **SARIF** output — when semantic rules ship
- **Color TTY output** — once a track decision lands

The defaults that ship today were chosen precisely so a YAML config is not required for useful CI feedback on day one.

---

## Status

v0.1 in active development. Shipped: five default rules, baselines, drift gate (DRIFT.1/2 gating + DRIFT.3 advisory), PR diff mode, advisory duplication report. Module-rule enforcement from YAML is the **v0.2** headline by design ([ADR-001](docs/decisions/001-config-rules-deferred-to-v0.2.md) — zero-config first); SARIF and the semantic backend are also v0.2. MVP acceptance criteria: [docs/MVP.md](docs/MVP.md) — the single open release item is security hardening on untrusted repos (#105).

---

## Secondary goal

Side experiment: test whether a useful, reliable, moderately complex CLI tool can be built end-to-end purely through agent conversation — no hand-written code.

---

## License

Apache License 2.0

---

## Principle

If it cannot be validated with tests — it should not exist.
