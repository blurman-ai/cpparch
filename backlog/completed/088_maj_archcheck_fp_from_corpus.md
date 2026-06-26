# 088 — archcheck v0.1 FP fixes from corpus stress-test

**Type:** maj. **Origin:** the structural census of 767 violators (1493 agentic C++) surfaced
systematic false-positives in SF.9 and duplication. Reports:
`docs/research/cpp_cycles_report.md`, `docs/research/cpp_copypaste_report.md`.

## Findings (FP classes)
1. **System `<...>` mis-resolution.** `<string.h>` (and other standard C headers)
   suffix-match a project file of the same name → phantom edge/cycle
   (PipeWire `defs.h ↔ string.h`). → `resolve_angle` must resolve standard
   system headers as External, without suffix-match.
2. **`.inl` idiom in SF.9.** The cycle `X.h ↔ X.{inl,ipp,tcc,tpp,tmpl.h,hxx}` of the same stem
   is a legitimate module split (header + inline-impl), the loop is broken by `#pragma once`/include-guard.
   SF.9 must not report this (legate 163 "cycles", acts 46 — almost all of it is this).
3. **Divergence `--graph sccs_cyclic` vs SF.9.** `--graph` gives the raw SCC count,
   SF.9 filters conditional (and now .inl). pyA2L: graph=22, SF.9=0. The census measured raw
   `--graph`, not SF.9 → overcounting. Bring into agreement / have the census measure by SF.9.
4. **Duplication: vendoring/generated code.** The density leaders are `argparse.hpp` (vendor),
   `bootstrap.c` (generated). The token detector does not separate vendored/generated code
   from a handwritten copy. → extend the mirror/generated filter to duplication.

> **Cross-check against the code (backlog review 2026-06-11):** 3 of 5 items are already implemented — marked below
> with commits. The live remainder of the task: #2.3 + a control re-run.

## Plan (one fix = one commit, fixture+test mandatory)
- [x] #2.1 resolve_angle: standard system headers → External (+ unit test). — implemented `9b3696e` (`is_system_header` in `src/scan/include_resolver.cpp`).
- [x] #2.2 SF.9: skip SCC = same-stem header+inline-impl (+ pass fixture+test). — implemented `8c05878` (`isInlineSplitScc` in `src/rules/sf9_no_cycles.cpp`). Don't confuse with the conditional filter from #032 (`04b523b`) — these are two independent mechanisms: #032 suppresses `#ifdef` cycles (spdlog), #2.2 — unconditional same-stem pairs (legate/acts).
- [x] #2.3 reconcile graph↔SF.9 / census by SF.9. — implemented 2026-06-13: `--graph` now outputs `sf9_cycles` (SF.9-filtered, actionable) next to `sccs_cyclic` (raw); exit code is now driven by `sf9_cycles`. Verified: pyA2L (raw=22, sf9=0, exit:0), spdlog (raw=22, sf9=0, exit:0), legate (raw=163, sf9=0, exit:0). 522/522 tests green.
- [x] #2.4 duplication: skip mirror/generated/vendored (+ test). — closed by other tasks: vendor-exclude centralized in #069 (`project_files.cpp`), generated-guard P0.9 `isGeneratedPath` from #065/#070 (`duplication_scanner.cpp`), whole-file twin suppression P0.2 (#070). Task #064 on the same topic closed as absorbed.
- [x] Re-run the FP repos: PipeWire, legate, acts, spdlog — all give sf9_cycles=0, exit:0 (2026-06-13).

## Acceptance
Each fix has a fixture/test; the FP repos from the reports stop being flagged; the tree
passes the gate (build+ctest+coverage); dogfooding archcheck on itself is green.

## How it works (DoD)

**#2.3 — sf9_cycles in --graph:**

`runGraph()` in `src/cli/preview_commands.cpp` now, after building the graph, runs `Sf9NoCycles::check()` and outputs two numbers:
- `sccs_cyclic` — the raw count of SCCs with size≥2 (as before, marked "raw")
- `sf9_cycles` — the number of violations that SF.9 actually reports (after the filters: .inl-split, conditional-include)

The `--graph` exit code is now determined by `sf9_cycles`, not the raw number. This makes `--graph` consistent with what `archcheck --check` produces.

**Closure status:** 2026-06-13. All 4 FP classes eliminated. 522/522 tests green.
