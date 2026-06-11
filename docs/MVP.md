# archcheck — MVP Scope (v0.1)

> Rewritten 2026-06-11 per task #045. The previous version of this document described
> the pre-#006/#028 design (compile_commands.json, config-driven dependency rules as
> core) that was deliberately rejected. Deferral decisions live in
> [docs/decisions/](decisions/); evidence behind the scope lives in
> [docs/research/full_audit_and_research_2026_06.md](research/full_audit_and_research_2026_06.md).

## Goal

**A trusted, zero-config architecture check and drift gate for C++.**

One command on a real repository must produce a short, believable report in seconds —
no build system, no compilation database, no YAML, no configuring "scary rules" up
front. Adoption friction is the #1 product risk: 30% of C++ developers use no code
analysis at all (JetBrains 2023), and the teams that needed this tool most (Figma,
Chrome) had to build it in-house. The MVP competes with *not using anything*.

## What v0.1 is

1. **Zero-config check** — `archcheck [path]`: five conservative default rules
   (SF.7, SF.8, SF.9, Lakos.GodHeader 50, Lakos.ChainLength 10), vendor/test/generated
   code excluded by default, attribution on every rule (Core Guidelines / Lakos).
2. **Legacy adoption in two commands** — `--save-baseline` freezes existing debt;
   `--baseline` fails only on new violations.
3. **Drift gate** — `--save-graph-baseline` once, then `--drift-baseline` in CI:
   gates **only** structural regressions (DRIFT.1 shortcut edges, DRIFT.2 cycle
   growth); legacy debt and advisory DRIFT.3 never fail the run.
4. **PR diff mode** — `--diff <revspec>`: deterministic structural regression report
   between two git refs.
5. **Honest config surface** — `.archcheck.yml` is validated (exit 2 on malformed)
   and `thresholds:` overrides apply. Module rules (`layers`/`independence`/
   `forbidden`) are parsed and validated but **not enforced** — that is the v0.2
   headline, by decision ([ADR-001](decisions/001-config-rules-deferred-to-v0.2.md)).

## Non-goals for v0.1 (each has a decision or a task)

- Module-rule **enforcement** from YAML — v0.2 ([ADR-001](decisions/001-config-rules-deferred-to-v0.2.md)).
- SF.21 and other semantic (AST) rules — v0.2+ ([ADR-002](decisions/002-sf21-deferred-to-v0.2.md)).
- libclang / `compile_commands.json` — v0.2 opt-in backend ([ADR-003](decisions/003-fast-backend-default.md)).
- Duplication as a gate — advisory only (`--duplication` always exits 0).
- SARIF, Martin metrics, color TTY, `archcheck init`.
- Cheap-drift heuristics wave (#093–#103) — post-release, demand-driven.

## Acceptance criteria

MVP is done when all of the following hold (status as of 2026-06-11 in brackets):

1. **Cold start**: bare `archcheck` on a mid-size real C++ repo (~1–2k files)
   finishes in seconds and reports a *reviewable* number of findings — dozens, not
   thousands. No config file involved. [✅ — corpus-validated; conservative
   thresholds + vendor/test exclusion are regression-tested]
2. **No known coarse-FP classes** in the default rule set; every FP class found on
   the corpus has a fix + regression fixture (#036, #047, #049, #058, #088). [✅]
3. **Legacy freeze works**: after `--save-baseline` + `--baseline`, an unchanged
   tree exits 0; a new violation exits 1. [✅]
4. **Drift gate is regression-only**: on a repo with hundreds of pre-existing
   violations, `--drift-baseline` exits 0 when the diff introduces no DRIFT.1/2,
   and 1 when it does. [✅ — validated live on LibreSprite PR #581]
5. **Deterministic output**: same input → byte-identical report (CI requirement). [✅]
6. **Honest contracts**: exit codes 0/1/2/3 all reachable in code; `--help` and
   README describe only shipped behavior; malformed config exits 2, never crashes. [✅
   as of 2026-06-11 — exit 3 catch-all, malformed-YAML fix, nonexistent-path fix]
7. **Fixtures for every shipped rule**: `pass/` and `fail_*/` per rule — the
   acceptance bar, not a nice-to-have. [✅ — 8/8 rules; duplication subsystem is
   advisory and unit-tested instead]
8. **Safe on untrusted repos**: a malicious repository cannot crash the process
   (SIGABRT/stack overflow) or read files outside the scan root. [◑ — S1/S2 fixed;
   S3–S6 tracked as #105, **release blocker**]
9. **Dogfood green in CI**: archcheck runs on its own `src/ include/ tests/` as a
   CI gate. [✅ as of 2026-06-11]

The single open item for a public v0.1 release is therefore **#105 (S3–S6 hardening)**
— everything else on this list is shipped and verified.

## Success condition

A maintainer of a mid-size C++ project can:

```bash
archcheck                                  # useful, short, trustworthy report — zero setup
archcheck --save-baseline base.json        # freeze legacy debt
archcheck --save-graph-baseline graph.yml  # snapshot the structure
# in CI:
archcheck --baseline base.json && archcheck --drift-baseline graph.yml
```

…and from that point on, CI fails **only** when a change makes the architecture
worse. No YAML written, no rules configured, no build integration.

## Why these criteria (evidence)

- **Zero-config wedge is the market-validated wedge**: Figma and Chrome built exactly
  this class of tool internally (include-graph checks in CI; Figma's blocks 50–100
  regressions/day) — without any module-rule DSL. Build times and dependency hygiene
  are the #1–2 chronic pains for ~80% of C++ developers (ISO surveys 2021–2026).
- **Narrow gates beat broad linting**: corpus analysis (135k commits) showed raw
  cross-area gating would false-alarm ~50% of the time, while cycle growth occurs in
  0.05% of commits — rare, objective, gate-grade. Gating only the narrow signals is
  what keeps the tool trustworthy (docs/research/drift_signal_validation.md;
  Lenarduzzi 2019: uncalibrated smell-gates don't correlate with faults).
- **First-run experience decides adoption**: the "5000 violations on first run"
  failure mode is why conservative defaults + baseline-from-day-one are acceptance
  criteria, not features (decision #028; ADR-001).

## Constraint (unchanged)

If a feature cannot be tested with fixtures — do not implement it.
