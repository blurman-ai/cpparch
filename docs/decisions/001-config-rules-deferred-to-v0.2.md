# ADR-001: Module-rule enforcement deferred to v0.2 (zero-config first)

**Status:** accepted (2026-05-28, task #028); reaffirmed with new evidence 2026-06-11
**Origin:** [backlog/completed/028_maj_rules_engine_mvp.md](../../backlog/completed/028_maj_rules_engine_mvp.md)

## Context

The original pitch ("dependency rules for C++") centered on YAML-declared module
contracts (`layers` / `independence` / `forbidden`). Implementing enforcement first
would have made the **first run** require: understanding the schema, mapping the
codebase into modules, writing globs — before seeing any value.

## Decision

v0.1 ships **zero-config intrinsic rules only** (SF.7/8/9, god-headers, chain length)
plus baseline and drift gating. The `.archcheck.yml` v1 schema is parsed and
validated, `thresholds:` overrides apply, but module rules are **not enforced** until
v0.2. `--help` states this explicitly.

## Rationale (original + 2026-06 research reinforcement)

- **Entry barrier kills adoption**: nobody describes modules on first contact with a
  tool. 30% of C++ developers use no code analysis at all (JetBrains 2023) — the
  competition is "nothing", and "nothing" has zero setup cost.
- The **market-validated value** of this tool class needs no config: Figma's and
  Chrome's in-house include-graph checkers (cycles, include weight) are exactly the
  zero-config feature set, and they paid for themselves (Figma: 50–100 blocked
  regressions/day). ISO surveys put build times/dependencies as the top chronic pains.
- **Drift gating doesn't need modules**: DRIFT.1/DRIFT.2 (the corpus-validated
  gate-grade signals) operate on the include graph alone.

## Consequences

- README/MVP acceptance criteria must not treat enforcement as a v0.1 gap — it is a
  scoped v0.2 headline (`docs/config_format.md` § Enforcement status).
- The v0.2 enforcement work consumes the already-shipped parsed config
  (`Config::modules`/`Config::rules`) — no schema work remains, only the rule classes.
- Marketing copy for v0.1 leads with "useful with zero setup", not with the DSL.
