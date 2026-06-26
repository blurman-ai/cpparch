# [GRAPH][RULES] Cheap graph checks: fan-out god-component + coupling/blast/scc-size in the report

**Created:** 2026-05-31
**Start date:** —
**Status:** new
**Module:** GRAPH / RULES / REPORT
**Priority:** major
**Complexity:** S (per item; each — a separate small commit)
**Blocks:** —
**Blocked by:** —
**Related:** #029 (metric_regression_detection — diff deltas of these same metrics), #037 (godheader_structural_threshold — fan-in threshold 50), #028 (rules_engine_mvp), #054 (ai_repo_duplication_run — the empirics where these signals came from)

## Goal

Add a set of architectural checks/metrics that are **almost free** — the number
is already computed by the graph engine (or obtained in one line from the already-built
graph), all that is missing is the "wiring" to a rule/report. No new backend,
no libclang, no git — everything on the already-working fast-include graph.

## Context

The corpus run #054 (85 repos, both metrics over history) showed which graph signals
actually distinguish drift — and **two of them archcheck computes but does not expose as
a check/metric**:

- **§7.3 "silent drift" — coupling `edges/nodes`.** stellar-core 1.9→6.3,
  opentelemetry 2.1→5.7, FastLED 1.3→4.8 — **zero cycles, low copy-paste, but
  average coupling tripled**. Right now this signal is nowhere visible: the edge counter
  exists (`GraphBuildResult.edges`, [graph_builder.cpp:27](../../src/graph/graph_builder.cpp#L27)),
  but `edges/nodes` is neither printed nor part of `GraphMetrics`.
- **§7.2 tangle size.** acts `hpp↔ipp` — these are 2-node cycles (cosmetic),
  FastLED — one SCC of 47 components (a real tangle). `largest_scc` is already
  computed (`compute_scc_stats`, [main.cpp:271](../../src/main.cpp#L271)) and
  printed in `--graph`, but SF.9 in the regular report says nothing about size.
- **§9 dedup techniques** — copy-paste along axes of variation (platforms, backends)
  spawns components that pull in dozens of siblings = **high fan-out**. We catch
  overload of the *input* (god-header, fan-in), but not overload of the *output*.

What already exists and we **do not duplicate**:
- `largest_scc` **growth** between revisions → **DRIFT.2** (shipped, spec:332
  "the size of an existing SCC grows").
- diff deltas NCCD / chain length / new god-headers → **#029**.
- CCD/ACD/NCCD, god-header (fan-in, threshold 50), chain length → shipped.

The spec already cites Lakos's "minimize **fan-in/fan-out**" (spec:191) and
"In-degree / **Out-degree** for each component" (spec:185) — i.e., fan-out
is covered by authority, just not implemented. This removes the "opinion rule" risk
(CLAUDE.md: authority over opinion).

## Scope

### A. New rule (absolute, single-snapshot) — the main item

- [ ] **`Lakos.GodComponentFanOut`** — a component with an out-degree (the number of direct
      `#include`s to project components) above a threshold. A mirror of `LakosGodHeaders`:
      the same shape, `predecessors()` → `successors()`. One rule file
      (`src/rules/lakos_god_component.cpp` + header), registration in `rule_set.cpp`,
      **without touching existing rules** (OCP).
  - Attribution: Lakos, "minimize fan-out" (the same source as god-header).
  - Threshold: do NOT copy 50 blindly — fan-out has a different distribution (a `.cpp` with
    30 includes is normal). Calibrate on the corpus `_aidev_run/` (already downloaded,
    see #054). Until calibration — a high default / report-only, so as not to give "5000
    violations on the first run" (CLAUDE.md).
  - Fixtures (mandatory): `fixtures/god_component_fanout/pass/` +
    `fixtures/god_component_fanout/fail_fanout/`.

### B. New numbers in the report (report-only, not pass/fail) — almost free

- [ ] **Average coupling `edges/nodes`** — add `edgeCount` + `avgCoupling`
      to `GraphMetrics` ([algorithms.h:25](../../include/archcheck/graph/algorithms.h#L25));
      `edgeCount` is already counted at graph build. Print next to CCD/ACD/NCCD.
- [ ] **Max blast radius** — `max_n |reachableFrom(n)|`. Already computed in the
      CCD loop ([algorithms.cpp:273](../../src/graph/algorithms.cpp#L273)) and
      **thrown away** (summed into CCD). Keeping the maximum — ~3 lines. This is the
      absolute version of the planned DRIFT.6 (blast_radius_growth).
- [ ] **SF.9: tangle size in the message.** When reporting a cycle, append the SCC size
      (how many components are in the tangle). Makes the finding actionable: "cycle of 2" vs
      "tangle of 47". An edit only in the SF.9 message formation, not in the logic.

### C. Drift versions — NOT here (deliberately split out)

The gates "coupling must not grow" / "blast radius must not grow" — that is metric
regression, the territory of **#029** (diff/baseline). After implementing B add
`avgCoupling` and `maxBlastRadius` to the `GraphMetrics` delta of #029 with one line each
(there is already nccdDelta there). In this task — only the absolute numbers.

### D. Docs

- [ ] **spec §Lakos**: add `edges/nodes` (avg coupling) and max blast radius to the
      metrics list; god-component (fan-out) — to the rules list next to god-header.
- [ ] **The coupling normalization decision** record in the spec (see Key
      decisions): we normalize `edges/nodes` (structurally), NOT per-KLOC. This is the
      only thing not in the docs right now.
- [ ] ROADMAP/CHANGELOG — on completion (not at creation), as required by
      `backlog/README.md`.

## Execution plan

1. [ ] B-metrics (`edgeCount`/`avgCoupling`/`maxBlastRadius` in `GraphMetrics` +
       printing) — the cheapest, unblocks both the report and the future #029 gate.
2. [ ] SF.9 message enrichment (SCC size).
3. [ ] `Lakos.GodComponentFanOut` + fixtures + unit test + registration.
4. [ ] Calibrate the fan-out threshold on `_aidev_run/`.
5. [ ] spec edits (A/B + the normalization decision).
6. [ ] (after) add `avgCoupling`/`maxBlastRadius` deltas to #029.

## Acceptance criteria

- [ ] `Lakos.GodComponentFanOut` exists, passes fixtures pass/fail, is registered,
      existing rules untouched (OCP).
- [ ] `--graph` and the regular report show `edges/nodes` and max blast radius.
- [ ] SF.9 in the message states the tangle size.
- [ ] The fan-out threshold is justified by numbers from the corpus, not pulled from thin air.
- [ ] Dogfooding: archcheck on itself stays green (CLAUDE.md).
- [ ] The spec reflects the new metrics/rule + the per-KLOC decision.
- [ ] Each item — a separate commit ≤50 lines, ≤2 new files (code_quality.md).

## Key decisions

| Decision | Reason |
|---------|---------|
| Normalize coupling as `edges/nodes`, NOT per-KLOC | per-KLOC mixes code volume into a structural metric (inflated LOC without edges → "improved" per-KLOC). `edges/nodes` is purely structural; it is exactly what gave the signal in #054 §7.3. per-KLOC would require a LOC counter, which the graph does not have |
| fan-out god-component — a separate rule, not an extension of god-header | OCP: one rule = one class = one file; god-header (fan-in, bottleneck) and god-component (fan-out, over-complicated consumer) — different defects |
| The fan-out threshold is calibrated on the corpus, conservative by default | We avoid "5000 violations on the first run"; a `.cpp` with 30 includes is normal |
| blast radius / coupling — report-only first | absolute thresholds need tuning; first show the number, the gate version (growth) — in #029/DRIFT |
| largest_scc growth NOT taken | already DRIFT.2 (shipped) |

## Done

- (empty)

## Pointers

- Signal empirics: [experiments/ai_repo_run/DRIFT_RUN_REPORT.md](../../experiments/ai_repo_run/DRIFT_RUN_REPORT.md) §7.2/7.3/9
- Graph metrics: [src/graph/algorithms.cpp](../../src/graph/algorithms.cpp) (`computeGraphMetrics`), [include/archcheck/graph/algorithms.h](../../include/archcheck/graph/algorithms.h)
- God-header (the template for the mirror): [src/rules/lakos_god_headers.cpp](../../src/rules/lakos_god_headers.cpp)
- `--graph` output (already prints largest_scc/edges): [src/main.cpp:258](../../src/main.cpp#L258)
- Diff deltas of metrics (where the C gates will go): #029, [src/diff/regression_report.cpp](../../src/diff/regression_report.cpp)
