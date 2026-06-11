# ADR-003: Fast preprocessor backend is the default; compile_commands.json optional

**Status:** accepted (2026-05-29, task #006; confirmed by perf spike #043)
**Origin:** [docs/dev/spike_clang_perf.md](../dev/spike_clang_perf.md), spec §«Двух-бекендная схема»

## Context

The original design read `compile_commands.json` and parsed translation units. That
makes the first run depend on the user's build system being configured — a hard
prerequisite exactly where adoption friction matters most.

## Decision

Two backends. The **fast preprocessor-only backend** (no compile DB, no libclang) is
the default and the only backend in v0.1. The **libclang backend** is opt-in
(`--with-clang`, v0.2) and exists solely for rules that genuinely need semantics.

## Rationale

- Measured cost gap (#043): on spdlog (141 TU) libclang single-thread ≈ 15 s vs
  regex/preprocessor baseline ≈ 11 ms — ×1350. On a ~5000-TU monorepo libclang-only
  ≈ 9 minutes, incompatible with a "seconds per PR" CI guard.
- Industry reference points confirm the split: clang-tidy's own
  `misc-header-include-cycle` runs at the preprocessor level (cycles need no
  semantics), while clangd's full Chromium index costs hours on a 48-core machine.
  Cheap-by-default, semantic-by-request matches how the ecosystem itself draws the line.
- Everything gate-grade in v0.1 (cycles, god-headers, chain length, SF.7/8, drift
  rules) is include/text-level — validated on the 310-repo corpus.

## Consequences

- `compile_commands.json` must not appear as a v0.1 requirement anywhere in docs.
- Semantic-rule candidates queue behind the v0.2 backend (ADR-002); the include
  backend must never grow AST-dependent heuristics as a workaround.
- The v0.2 backend is per-TU incremental and opt-in — cost model documented in
  [docs/research/cognitive_complexity_delta_design.md](../research/cognitive_complexity_delta_design.md) §6
  and the clang-backend section of the 2026-06 research digest.
