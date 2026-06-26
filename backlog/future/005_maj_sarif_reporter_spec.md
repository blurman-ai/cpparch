# [REPORT] SARIF reporter specification

**Created:** 2026-05-26
**Started:** —
**Status:** new
**Module:** REPORT
**Priority:** major
**Complexity:** S (half a day for the spec, implementation as a separate task)
**Target release:** v0.2+ (SARIF output lands in v0.2 per roadmap)
**Blocks:** sarif_reporter implementation (a separate task later)
**Blocked by:** —
**Related:** #001 (dogfood_static_analyzers — the current strict report could also move to SARIF), #6 (gh — audit Issue 7: phase is ambiguous)

## Goal

Pin down **before writing any reporter code** the mapping of archcheck's internal concepts (rules, violations, sources: Core Guidelines / Lakos / Martin) into SARIF 2.1.0, so the format doesn't have to be reworked after the first release.

## Context

SARIF 2.1.0 — OASIS standard, JSON, understood by GitHub Code Scanning, SonarQube, GitLab, and the VS Code extension `MS-SarifVSCode.sarif-viewer`. It is the "lingua franca" for integrating with the partner ecosystem (clang-tidy, cppcheck, GCC `-fanalyzer` already speak SARIF). Lay the groundwork from v0.1.

Spec: https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html

## Execution plan

- [ ] **Decide the target phase.** Audit #6 Issue 7: the roadmap (`architecture-spec.md` line 656) places SARIF in v0.2, while this task lives in `future/` (by convention = v0.3+). In parallel, the "SARIF from v0.1" declaration in "Key decisions" below is a third variant. Make the decision explicit: v0.2 → move the file to `new/` and create a paired implementation task; v0.3+ → update the roadmap in the spec. Exactly one formulation should remain.
- [ ] Decide the `ruleId` format: proposal — `archcheck/core-guidelines/SF.7`, `archcheck/lakos/no-cycles`, `archcheck/martin/instability`. Authority = namespace, rule name = last segment
- [ ] `tool.driver.name` = `archcheck`, `version` = from CMake
- [ ] `rules[]` section: for each rule — `id`, `name`, `shortDescription`, `fullDescription`, `helpUri` (link to the Core Guidelines / Lakos chapter), `defaultConfiguration.level`
- [ ] `results[].level`: mapping of our severities → SARIF (`error` / `warning` / `note`)
- [ ] `results[].locations[]`: file/line/column from the `file:line:column` contract in the spec
- [ ] Graph violations (cycle) — as `relatedLocations[]`, all participants of the cycle
- [ ] Metrics (CCD/ACD/NCCD) — **not via results**, but via `properties` at the run level (or a separate `invocations[].properties`) — to discuss
- [ ] Baseline mode — mapping to SARIF `baselineState` (`unchanged` / `new` / `absent`)
- [ ] Put the final spec with JSON examples in `docs/sarif-mapping.md`
- [ ] Prepare 2-3 reference files (`tests/sarif/expected_*.json`) before implementation

## Done
- (empty)

## In progress
- (empty)

## Next steps

1. Read the SARIF 2.1.0 spec section by section (rules, results, locations, baseline)
2. Look at how clang-tidy and cppcheck serialize — take it as a model
3. Write `docs/sarif-mapping.md`

## Key decisions

| Decision | Reason |
|----------|--------|
| SARIF from v0.1 | a free ticket into GitHub Code Scanning + SonarQube |
| ruleId with namespace by authority | preserves the "authority-cited rules" positioning in the output |

## Changed files

| File | Change |
|------|--------|
| docs/sarif-mapping.md | new spec |
| tests/sarif/expected_*.json | reference outputs |
