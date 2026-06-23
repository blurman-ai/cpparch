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

**archcheck keeps the hard CI gate narrow, and reports the rest as explicit advisories.**

---

## What it does (today, v0.1)

- Scans `.h` / `.cpp` sources with a fast preprocessor pass — no `compile_commands.json` required
- Builds the include-dependency graph and detects cycles, deep chains, and god-headers
- Runs five default intrinsic rules sourced from C++ Core Guidelines and Lakos:
  - **SF.9** — no cycles in the include graph (**gating** in plain check mode)
  - **SF.7** — no `using namespace` in headers (**advisory**)
  - **SF.8** — every header has `#pragma once` or include guard (**advisory**)
  - **Lakos.GodHeader** — fan-in ≤ 50 incoming includes (**advisory** in check mode)
  - **Lakos.ChainLength** — include-chain depth ≤ 10 (**advisory**)
- Reports findings as `file:line: [rule] message`; exit `1` means a gated finding, not merely "anything was reported"
- Tracks architectural drift between graph baselines: `DRIFT.1`, `DRIFT.2`, and `DRIFT.4.CYCLE` gate; `DRIFT.3`, `DRIFT.4.NEW`, `DRIFT.4.SDP`, and pre-existing findings are advisory
- Runs the canonical PR workflow with `--diff <revspec>`: new/grown cycles and new god-headers gate, while added edges, chain/NCCD growth, SATD, test co-evolution, local complexity, flag-argument drift, and new clone drift are advisory

The current signal model:

| Layer | Examples | Exit behavior |
|-------|----------|---------------|
| Core gate | SF.9 cycles, DRIFT.1/2/4.CYCLE, `--diff` new/grown cycles and new god-headers | exit `1` |
| Structural advisories | SF.7/SF.8, Lakos chain/god-header in check mode, added edges, NCCD/chain growth | reported, exit `0` |
| PR hygiene advisories | SATD, test co-evolution, local complexity, flag arguments, new clones | reported, exit `0` |
| History analytics | `--history` god-file growth and defect-attractor signals | report-only, exit `0` |

---

## Quick start

```bash
# Default check on current directory — zero config, no flags
archcheck

# Check a specific path
archcheck path/to/src

# JSON output for CI integration. Check-mode JSON includes top-level
# "gate": "ok|fail" and per-finding "disposition": "gating|advisory".
archcheck --format json src/

# PR check — the canonical CI workflow. Exit 1 only on gated regressions
# (new/grown cycles, new god-headers); structural and hygiene drift are
# reported as advisory and never fail the run.
archcheck --diff origin/main..HEAD .
archcheck --diff --format=json origin/main..HEAD .   # machine-readable

# Freeze legacy violations, fail only on new ones
archcheck --save-baseline archcheck-baseline.json src/
archcheck --baseline      archcheck-baseline.json src/

# Alternative to --diff when you want to pin a reviewed graph snapshot in a
# file (instead of comparing two git refs): snapshot once, gate drift in CI
archcheck --save-graph-baseline graph.json src/
archcheck --drift-baseline      graph.json src/

# Validate .archcheck.yml and apply threshold overrides
archcheck --config .archcheck.yml src/

# Advisory reports (report-only, never gate)
archcheck --duplication src/
archcheck --history src/

# Quick previews: scanner stats / include-graph stats
archcheck --scan src/
archcheck --graph src/
```

A ready-to-copy GitHub Actions job for the PR workflow (sticky PR comment +
step summary) lives in
[`.github/workflows/example_archcheck_pr.yml`](.github/workflows/example_archcheck_pr.yml).

### Default thresholds

These thresholds apply automatically without a config file. Override any via the `thresholds:` block in `.archcheck.yml`:

| Threshold | Default value | Rule / subsystem |
|-----------|---------------|------------------|
| include chain length | `10` | `Lakos.ChainLength` |
| god-header fan-in | `50` | `Lakos.GodHeader` |
| project source extensions | `.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .tpp .inl .inc` | scan |
| header extensions | `.h .hh .hpp .hxx .ipp .tpp .inl .inc` | scan |

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
- **CI-first** — deterministic output; non-zero exit only for gated findings
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

archcheck is not a clang-tidy replacement. Its hygiene signals are diff-scoped,
advisory-first regression hints for CI, not broad style or semantic linting.

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

v0.1 in active development. Shipped: five default rules, baselines, graph drift gate
(`DRIFT.1/2/4.CYCLE` gating + `DRIFT.3/4.NEW/4.SDP` advisory), PR diff mode,
check/diff JSON, advisory duplication and history reports. Module-rule enforcement
from YAML is the **v0.2** headline by design ([ADR-001](docs/decisions/001-config-rules-deferred-to-v0.2.md)
— zero-config first); SARIF and the semantic backend are also v0.2. Current release-readiness
status lives in [backlog/TASK_TRACKER.md](backlog/TASK_TRACKER.md).

---

## Secondary goal

Side experiment: test whether a useful, reliable, moderately complex CLI tool can be built end-to-end purely through agent conversation — no hand-written code.

---

## License

Apache License 2.0

---

## Principle

If it cannot be validated with tests — it should not exist.
