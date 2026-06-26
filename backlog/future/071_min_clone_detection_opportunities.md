# [RULES][SCAN][REPORT] Clone detection pain points & opportunities (LD.10–LD.18)

**Created:** 2026-06-02
**Started:** —
**Status:** new
**Module:** RULES / SCAN / REPORT
**Priority:** minor
**Complexity:** unknown (umbrella — each LD.* is estimated separately)
**Target release:** v0.3+
**Blocks:** —
**Blocked by:** —
**Related:** #052 (cross_tu_duplication_detector), #053 (line_duplication_pass), #056 (token_duplication_pass), #054 (ai_repo_duplication_run), #065 (dedup_skip_generated), #070 (precision/FP fixes), #033 (ai_drift_dataset)
**Spin-off:** #072 (clone_type_classifier — LD.10 + LD.11 implemented in spike #056)

## Goal

Collect into one research backlog the pain points of CPD-style clone detectors and the opportunities that would set archcheck apart from "found a duplicated block" — classification, explanation, categories, growth metrics, and an architectural cut of duplication.

## Context

Inspiration sources: SonarSource Community discussions, StackOverflow questions, clone detection research, common complaints about CPD tools.

Status — **future ideas, not MVP**. This is a layer on top of the already-existing duplication subsystem: complementary layers (#053 line / #056 token / #052 AST / #070 precision / #054 usage). The single source of truth is [docs/duplication_architecture.md](../../docs/duplication_architecture.md); read it before working on any LD.* item. Some items directly continue the ongoing precision/FP work (#070, #065) — they're worth picking up as "free" side effects of that branch.

Before pulling any item into implementation — check it against the "What it explicitly is NOT" list in [CLAUDE.md](../../CLAUDE.md): we don't turn archcheck into a GUI/dashboard, we stay CLI+CI.

### Idea catalog

**LD.10 — Clone Classification.** Detectors report a "duplicated block" but don't distinguish the type: exact / identifier-renamed / literal-only / structural. Without the type the developer doesn't understand whether the clone is dangerous or harmless. Output: `Clone Type: Exact|Identifier-Renamed|Literal-Only|Structural`. Value: fewer FP complaints, easier triage and prioritization. (We already have selective normalization in the token pass — the clone type is largely derived from *what* was normalized at the match.)

**LD.11 — Clone Explanation.** Tools show "Duplicated block detected" without explaining *why*. Developers argue with the result, not understanding what matched. Output: matched tokens, ignored identifiers (`userId -> accountId`), ignored literals (`100 -> 200`), similarity %. Value: trust in the result, easier debugging of the detection logic, easier adoption. **High interest.**

**LD.12 — Clone Categories.** Not all clones are equally harmful: generated code, DTOs, protobuf structures, ORM entities, lookup tables trigger reports but rarely = architectural debt. Categories: Generated / Data Mapping / Test Code / Business Logic / Unknown. Value: signal-to-noise, focus on real maintenance cost. (Overlaps with #065 dedup_skip_generated — there generated is already cut; here — an extension to classification, not just skip.) **Research required.**

**LD.13 — Micro-Clones.** Large thresholds hide small duplicates: validation, retry, logging, exception handling. Output: `Micro Clone / Lines: 5 / Occurrences: 7`. Value: early detection, refactoring opportunities, useful for AI-generated code. **Research required** (risk of FP explosion at a low threshold — requires careful validation on the corpus, cf. #070/#066).

**LD.14 — Clone Growth.** ✅ **Done in spike #056** (commits `906b92c` density-summary, `dea0e7c` growth-gate). Tools answer "how much duplication there is" but not "how much was added". Metrics: Clone LOC Delta, Clone Density Delta, Clone Block Delta. Example: `Before 3.1% → After 3.8% → Delta +0.7%`. Implemented: snapshot prints `clone density: cloneLOC / totalLOC (P%)`; `--clone-baseline <path>` persists density, on the next run yields a delta and `exit 1` on growth above `--clone-growth-max` (CI gate). Value: CI gating, trend tracking, architectural drift signal. **High interest.**

**LD.15 — Diff-Induced Clones.** "Where did this clone come from?" — tools rarely link clones to change history. Output: `Clone introduced in: commit abc123`, source/target files. Value: accountability, easier review, root-cause. (Requires git-blame integration — check against "not a GUI"; this is CLI report enrichment, acceptable.) **High interest.**

**LD.16 — Cross-Module Clone Matrix.** ✅ **Done in spike #056** (commits `fc70ec0` cross-flag, `6e74a9b` matrix). Duplication across subsystem boundaries is more dangerous than intra-module. Output: matrix `Navigation -> Weather: 320 LOC`. Implemented: `moduleOf` (leading path segment) → snapshot prints `cross-module: K of M pairs cross a boundary` + matrix `mod <-> mod: N pairs, L LOC`. Value: architectural-drift signal, visibility of subsystem coupling. (Right in the spirit of Lakos physical design — the most "architectural" of the ideas.) **High interest.**

**LD.17 — Clone Hotspots.** Some files constantly become sources of clones. Output: `Top Clone Sources: config.cpp exported 14 clones`. Value: refactoring targets, erosion tracking. **Medium interest.**

**LD.18 — AI Clone Fingerprints.** AI code breeds similar patterns across the whole repo; traditional detection sees the symptoms but not the pattern. Output: `Repeated Pattern: RetryLoopV1 / Occurrences: 17 / Modules: 8`. Value: monitoring AI-assisted development, drift analysis, repository hygiene. (Connection to #054 ai_repo_duplication_run and #033 ai_drift_dataset — duplication = the #1 AI-drift signal.) **Research required.**

### Priorities (from the source)

- **High interest:** LD.11 (explanation), LD.14 (growth), LD.15 (diff-induced), LD.16 (cross-module matrix).
- **Medium interest:** LD.10 (classification), LD.17 (hotspots).
- **Research required:** LD.12 (categories), LD.13 (micro-clones), LD.18 (AI fingerprints).

## Execution plan

- [ ] Re-read [docs/duplication_architecture.md](../../docs/duplication_architecture.md) and record which of the LD.* are derived "for free" from the current pipeline (clone type, matched tokens, ignored ids/literals — already computed at the match).
- [ ] For each High-interest item (LD.11/14/15/16) — a separate task in `backlog/future/` with fixtures and a complexity estimate, when its turn comes.
- [ ] LD.12/13/18 — first a research note (FP risks, corpus volume), then a go/no-go decision.
- [ ] Check each candidate against "What it is NOT" (CLI+CI, not a dashboard) before promoting from future/.

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. ~~LD.10 (classification) + LD.11 (explanation)~~ — **done in spike #056** (see #072): the data was in the matcher, classifier + explain on top of `rawSeq`/`diffTokens`.
2. ~~LD.14 (growth)~~ — **done in spike #056** (`906b92c`/`dea0e7c`): density-summary + `--clone-baseline` growth-gate with epsilon-stable comparison.
3. ~~LD.16 (cross-module matrix)~~ — **done in spike #056** (`fc70ec0`/`6e74a9b`): `moduleOf` + cross-flag + boundary matrix. The most "Lakos-native" item, a candidate for a flagship v0.3 feature.

**Done (spike #056):** LD.10, LD.11, LD.14, LD.16. **Remaining High/Medium:** LD.15 (diff-induced, git-blame), LD.17 (hotspots). **Research:** LD.12, LD.13, LD.18.

## Key decisions

| Decision | Reason |
|---------|---------|
| One umbrella task, not 9 separate ones | Future research; split into tasks as they're promoted High → Medium → Research |
| Put in `future/`, release v0.3+ | Explicit directive "for future research"; the duplication subsystem is still in v0.1/v0.2 (#052–#070) |
| LD.11/10 derived from the matcher, not new infra | selective normalization already knows matched tokens + ignored ids/literals |

## Changed files

| File | Change |
|------|-----------|
| (none yet) | research phase |

## Fixtures (if a rule)

- [ ] `fixtures/clone_classification/` — exact / renamed / literal-only / structural (LD.10)
- [ ] `fixtures/clone_explanation/` — pairs with known ignored ids/literals (LD.11)
- [ ] `fixtures/clone_growth/` — before/after snapshots for delta metrics (LD.14)
- [ ] `fixtures/cross_module_clones/` — duplicates across module boundaries (LD.16)
