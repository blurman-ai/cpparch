# [RULES][GRAPH] DRIFT.4 lateral_module_dependency — product rule for lateral drift

**Created:** 2026-06-12
**Started:** 2026-06-12
**Completed:** 2026-06-12
**Status:** done
**Module:** RULES][GRAPH
**Priority:** major
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #111/#115/#117 (criterion validation, completed), #087 (DRIFT.3 — sample of area logic), #057 (Lakos fan-out checks)

> All file:line anchors were taken 2026-06-12 — recheck with grep at start.

## Goal

Port the validated criterion for lateral cross-module drift
([docs/research/lateral_module_drift_criterion.md](../../docs/research/lateral_module_drift_criterion.md),
run: [lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md))
into the product rule `DRIFT.4 lateral_module_dependency`: CYCLE grade gates,
SDP/NEW — advisory.

## Context

The criterion was validated on 479 repos: 495 events out of 21,736 raw (108× suppression),
eyeball TP 85%, CYCLE precision 92%. The agentic cut — a null-result (not for the rule,
for science). In CI mode the two crutches of the corpus run are NOT needed: the baseline is taken from
the saved graph (not by reconstruction), persistence isn't needed (the gate stands before
merge).

**Numbering decision (made, see "Key decisions"):** the rule takes **DRIFT.4**;
the planned wave in the spec (`DRIFT.4 public_surface_growth` … `DRIFT.9 hotspot_inflow`,
[architecture-spec.md:379-384](../../docs/architecture-spec.md)) is renumbered into
DRIFT.5–DRIFT.10. There's already a precedent: the spec did the same when DRIFT.3 took
bidirectional coupling (#087) — see the spec changelog, line ~6.

## Solution architecture (for the implementer — step by step)

### Step 1. Rule class (2 new files)

Following DRIFT.3 ([drift_bidirectional_coupling.h:23-33](../../include/archcheck/rules/drift_bidirectional_coupling.h#L23-L33)):

- `include/archcheck/rules/drift_lateral_module_dependency.h`:
  ```cpp
  class DriftLateralModuleDependency final : public IRule
  {
  public:
    explicit DriftLateralModuleDependency(graph::DependencyGraph baseline);
    ViolationList check(const graph::DependencyGraph &graph,
                        const std::function<std::string(std::string_view)> &readFile) const override;
    std::string_view id() const override { return "DRIFT.4"; }
  private:
    graph::DependencyGraph baseline_;
  };
  ```
- `src/rules/drift_lateral_module_dependency.cpp` — logic (Step 3).

The `IRule` interface: [i_rule.h:13-22](../../include/archcheck/rules/i_rule.h#L13-L22).
`Violation{ruleId, file, line, message}`: [violation.h:9-15](../../include/archcheck/rules/violation.h#L9-L15);
for the graph level `line = 0`.

### Step 2. Grades via the ruleId suffix

Gating in check mode matches the ruleId string
([check_command.cpp:100-109](../../src/cli/check_command.cpp#L100-L109)):
```cpp
const auto gating = std::count_if(..., [](const auto &v)
    { return v.ruleId == "DRIFT.1" || v.ruleId == "DRIFT.2"; });
```
The rule emits Violations with **three different ruleIds**: `DRIFT.4.CYCLE`, `DRIFT.4.SDP`,
`DRIFT.4.NEW` (the `id()` method returns `"DRIFT.4"` — that's the rule identifier,
the finding's ruleId carries the grade). To the gate condition add `|| v.ruleId == "DRIFT.4.CYCLE"`
and update the message text `"(DRIFT.1/DRIFT.2)"` → `"(DRIFT.1/DRIFT.2/DRIFT.4.CYCLE)"`.

### Step 3. The check() algorithm — exact port of the criterion

The module of a file: **reuse `areaOf` from DRIFT.3**
([drift_bidirectional_coupling.cpp:67-89](../../src/rules/drift_bidirectional_coupling.cpp#L67-L89) —
skip wrapper-dirs `src/include/lib`, noise-dirs `build/test/vendor`, the first significant
component; the noise constants are there too, lines 23-44). Extract `areaOf` into a shared header
(e.g. `include/archcheck/rules/area_of.h`) — do NOT copy-paste (dedup rule).
Note: the research prototype used auto-depth (≥3 siblings) — this refinement is
deferred, v1 deliberately takes the shipped area logic.

On the **baseline graph** compute once (everything on direct edges — NOT transitive,
see criterion §2 "caveat"):

```
modEdges  : set<pair<Area,Area>>      — all cross-module pairs of the baseline
FID[B]    = |{A != B : (A,B) in modEdges}|
FOD[B]    = |{C != B : (B,C) in modEdges}|
I[M]      = FOD-edges-outward / (incoming + outgoing)   // Martin, at the module level
maxFID    = max over modules
medianFOD = median over modules
shared(B) = FID[B] >= 0.5 * maxFID  &&  FOD[B] <= medianFOD
```

For each edge from `addedEdges(baseline_, graph)`
([graph/diff.h:21](../../include/archcheck/graph/diff.h#L21)):

1. `A = areaOf(from), B = areaOf(to)`; skip if A or B is empty, or A == B;
2. skip if `B` is absent from the baseline as a module (module birth — not drift;
   CYCLE is impossible for it by construction, metrics are undefined);
3. skip if `shared(B)`;
4. skip if the pair `(A,B)` is already in `modEdges` (not the first link);
5. **mass-touch guard**: if `addedEdges` total > 150 — the rule stays silent entirely
   (constant `kMassTouchEdges = 150` with a comment referencing criterion §3);
6. grade:
   - `(B,A) ∈ modEdges` → **DRIFT.4.CYCLE** (the back-edge is exact — the baseline graph
     is real, phantoms like in #117 don't happen here by construction);
   - `I[B] > I[A] + 0.10` → **DRIFT.4.SDP** (`kSdpDelta = 0.10`);
   - otherwise → **DRIFT.4.NEW**.
7. One event per pair (A,B) per run — dedup; message:
   `"module 'A' -> 'B': first lateral dependency (via <from> -> <to>)"`,
   `file = from`, `line = 0`.

The constants (`kSharedFidRatio = 0.5`, `kSdpDelta = 0.10`, `kMassTouchEdges = 150`) —
named in the .cpp with a one-line comment and a link to the research doc. Do NOT
move them into `config::Thresholds` (YAGNI; config plumbing — when a user asks).

### Step 4. Registration

[rule_set.cpp:26-33](../../src/rules/rule_set.cpp#L26-L33), one line in
`makeDriftRuleSet` (the baseline is copied into each rule, the last one — move; insert
BEFORE the move line):
```cpp
rules.push_back(std::make_unique<DriftLateralModuleDependency>(baseline));
```

### Step 5. Fixtures (mandatory — MVP.md)

Directory `fixtures/drift_lateral/`, format like `drift_shortcut_edge/`
(`baseline.graph.yml` + headers; yaml example — fixtures/drift_shortcut_edge/pass/):

| Fixture | Baseline | Current state | Expectation |
|---|---|---|---|
| `pass_shared_target/` | a/,b/,c/ all → utils/ (FID(utils)=3=max, FOD=0) | d/x.h added `#include "utils/u.h"` | 0 findings (B shared) |
| `pass_existing_pair/` | edge a/x.h → b/y.h already exists | a/z.h → b/y.h (second link of the pair) | 0 findings |
| `fail_lateral_new/` | a/, b/ independent, both at the same level | a/x.h → b/y.h | DRIFT.4.NEW |
| `fail_lateral_cycle/` | b/y.h → a/w.h already exists | a/x.h → b/z.h | DRIFT.4.CYCLE |
| `fail_lateral_sdp/` | I(b) noticeably > I(a): b depends on 3 modules, a — leaf with incoming | a/x.h → b/y.h | DRIFT.4.SDP |

In each fixture's baseline-yml keep ≥3 modules, so that maxFID/median don't degenerate.

### Step 6. Tests

- Unit `tests/unit/rules/drift_lateral_module_dependency_test.cpp` — synthetic
  graphs (sample: drift_no_cycle_growth_test.cpp): shared threshold (exactly 0.5·max — boundary),
  median FOD, the grace of a new module (item 2), mass-touch, pair dedup, all three grades.
- Integration: `tests/integration/rules/drift_fixtures_test.cpp` —
  `run_drift_check()` (lines 63-81) already calls `makeDriftRuleSet`, add cases
  for 5 fixtures.

### Step 7. Documentation and numbering

- `docs/architecture-spec.md`: renumber the DRIFT.4–9 wave (lines ~379-384) into
  DRIFT.5–10; in the spec changelog add a line "ID DRIFT.4 taken by the shipped
  lateral_module_dependency (#118), the wave became DRIFT.5–DRIFT.10" (following the
  existing entry about DRIFT.3). Note: #057 references "DRIFT.4
  (blast_radius_growth)" — fix to the new number.
- `CHANGELOG.md` (Keep a Changelog, Unreleased/Added).
- `--help`: if the DRIFT rules are listed there — add a line.
- In the drift-mode gate message (check_command.cpp:104-106) mention DRIFT.4.CYCLE.

### Step 8. Self-check

- `cmake --build build/debug && cd build/debug && ctest --output-on-failure`
- lizard gate: `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/`
- Dogfood: `./build/debug/src/archcheck` from the root — `src/ include/ tests/` give 0 violations
- Commits of ≤50 lines: (1) area_of extraction, (2) rule+factory, (3) gating,
  (4) fixtures+tests, (5) docs/spec.

## Execution plan

- [ ] Extract `areaOf` into a shared header (refactor DRIFT.3, behavior bit-for-bit, tests green)
- [ ] Class DriftLateralModuleDependency + module metrics on the baseline
- [ ] Grades + DRIFT.4.CYCLE gating in check_command
- [ ] 5 fixtures + unit + integration tests
- [ ] Spec (wave renumbering) + CHANGELOG + #057 fix
- [ ] Self-check (build+tests, lizard, dogfood)

## Done

- `include/archcheck/rules/area_of.h` — extraction of areaOf from DRIFT.3
- `src/rules/drift_bidirectional_coupling.cpp` — switch to the shared area_of.h
- `include/archcheck/rules/drift_lateral_module_dependency.h` — rule class
- `src/rules/drift_lateral_module_dependency.cpp` — check(), computeMetrics(), isShared() (strict `>` fix for int truncation on small graphs), gradeEdge(), buildMsg()
- `src/rules/rule_set.cpp` — registration in makeDriftRuleSet
- `src/cli/check_command.cpp` — DRIFT.4.CYCLE in gating
- `src/CMakeLists.txt` — +drift_lateral_module_dependency.cpp
- `fixtures/drift_lateral/` — 5 fixtures (pass×2, fail_new/cycle/sdp)
- `tests/unit/rules/drift_lateral_module_dependency_test.cpp` — 6 unit tests
- `tests/integration/rules/drift_fixtures_test.cpp` — +5 integration cases
- `docs/architecture-spec.md` v2.3 — DRIFT.4–9 → DRIFT.5–10, wave renumbered
- `CHANGELOG.md` — Unreleased/Added
- `backlog/new/057_*.md` — fix DRIFT.4→DRIFT.6 for blast_radius
- Self-check: 519/519 tests, lizard ok, dogfood 0 violations

## In progress

- (empty)

## Next steps

1. Start with extracting areaOf — it's a separate safe commit.

## Key decisions

| Decision | Reason |
|---------|---------|
| ID = DRIFT.4, spec wave shifts into DRIFT.5–10 | Precedent: already done for DRIFT.3 (#087); research docs already call the rule DRIFT.4 |
| Grade in the ruleId suffix (DRIFT.4.CYCLE/SDP/NEW) | Gating in check_command matches the ruleId as a string — extension without new mechanisms |
| areaOf from DRIFT.3, not auto-depth from the prototype | Boring tech: shipped and tested logic; auto-depth is a separate improvement on request |
| Constants in .cpp, not in config::Thresholds | YAGNI; thresholds validated by the corpus, user-config — when asked |
| Drift mode only (makeDriftRuleSet), don't touch diff mode | RegressionReport is a separate mechanism without IRule; a lateral section there is a separate task if needed |
| Persistence/grace period is not ported | In CI baseline = exact graph, the gate is before merge — the corpus crutches aren't needed (research doc §7) |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/rules/drift_lateral_module_dependency.h` | New |
| `src/rules/drift_lateral_module_dependency.cpp` | New |
| `include/archcheck/rules/area_of.h` | Extraction of areaOf from DRIFT.3 |
| `src/rules/drift_bidirectional_coupling.cpp` | Switch to the shared areaOf |
| `src/rules/rule_set.cpp` | +1 factory line |
| `src/cli/check_command.cpp` | DRIFT.4.CYCLE gating |
| `fixtures/drift_lateral/*` | 5 fixtures |
| `tests/unit/rules/drift_lateral_module_dependency_test.cpp` | New |
| `tests/integration/rules/drift_fixtures_test.cpp` | +5 cases |
| `docs/architecture-spec.md`, `CHANGELOG.md`, `backlog/new/057_*.md` | Numbering/docs |

## Fixtures

- [ ] `fixtures/drift_lateral/pass_shared_target/`
- [ ] `fixtures/drift_lateral/pass_existing_pair/`
- [ ] `fixtures/drift_lateral/fail_lateral_new/`
- [ ] `fixtures/drift_lateral/fail_lateral_cycle/`
- [ ] `fixtures/drift_lateral/fail_lateral_sdp/`

## How it works (summary)

**Principle.** `DriftLateralModuleDependency::check()` takes `addedEdges(baseline_, graph)`
(new edges compared to the graph-baseline), reduces each edge to a pair of modules via
`areaOf()`, and emits a graded violation for the first lateral link between equal-rank
modules. On the baseline the module metrics (FID/FOD) are computed once: `computeMetrics()`.

**Grades (ruleId suffix).** `gradeEdge()`:
- `DRIFT.4.CYCLE` — the baseline has a reverse edge (B→A): the new link A→B closes
  a module cycle. **Gates** (exit ≠ 0 in `--drift`).
- `DRIFT.4.SDP` — violation of Martin's Stable Dependencies: I(B) > I(A) + 0.10. Advisory.
- `DRIFT.4.NEW` — first lateral pair without a cycle/SDP. Advisory.

**Filters (silence).** Mass-touch guard: >150 added edges → reorg/vendor, the rule
stays silent entirely. Shared layer (`isShared`: FID > 0.5·maxFID AND FOD ≤ medianFOD) — infrastructure,
not drift. Birth of module B (not in baseMetrics) — not drift. Pair already in baseline — not the first.

**Subtlety (a bug that was caught).** `isShared` uses a **strict** `>`, not `>=`:
`static_cast<int>(0.5·maxFID)` truncates (0.5·3=1.5→1), and with `>=` a module with FID=1 in a small
graph was falsely counted as shared, muting CYCLE/NEW. The strict `>` fixes this.

## What controls it

Constants in `src/rules/drift_lateral_module_dependency.cpp` (NOT in config — YAGNI):
`kSharedFidRatio = 0.50`, `kSdpDelta = 0.10`, `kMassTouchEdges = 150`. The thresholds are
validated by the corpus (#111/#115/#117). Registration — `makeDriftRuleSet` in `rule_set.cpp`
(drift mode only). CYCLE gating — string match of ruleId in `check_command.cpp`.

## What it relates to

Source of the criterion — `docs/research/lateral_module_drift_criterion.md` + the run
`lateral_module_drift_corpus_run.md`. `areaOf` is shared with DRIFT.3 (#087) via
`include/archcheck/rules/area_of.h`. The Python twin of the rule (the corpus scanner) —
`experiments/ai_repo_run/lateral_drift_scan.py` (#119/#121). Spec: the DRIFT.5–10 wave.

## Diagnostics

Tests: `tests/unit/rules/drift_lateral_module_dependency_test.cpp` (6),
`tests/integration/rules/drift_fixtures_test.cpp` (5 fixtures `fixtures/drift_lateral/`).
Run pointwise: `ctest -R "DRIFT.4|drift.*lateral"`. The finding message contains
`(via <from> -> <to>)` — the concrete file-edge that gave birth to the module pair.
