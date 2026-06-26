# [RULES][SCAN] Cross-TU duplication detector (missing reuse edge)

**Created:** 2026-05-29
**Started:** —
**Status:** new
**Module:** RULES / SCAN
**Priority:** major
**Difficulty:** L (multi-day, splits into Stage 0 → 1 → 2, gate after Stage 0)
**Target release:** v0.2
**Blocks:** —
**Blocked by:** #042 (clang_semantic_backend — Stage 2 sits on libclang infra), #043 (spike_clang_perf — may change the overall backend choice)
**Related:** #006 (spec_refactor — two-backend scheme), #033 (ai_drift_dataset — duplication = the #1 AI-drift signal)

> **2026-05-29:** deferred to `future/` together with #042. Stage 2 sits on the libclang infrastructure, which is itself in future/. Stage 0 (research/proof) can be pulled earlier if needed — it is independent of the backend.

## Goal

Set up an optional "cross-TU duplication detector" pass on top of the libclang backend: find functions/blocks rewritten in one module instead of being reused from another. Classified as a **missing reuse edge** — a physical-design defect that the dependency graph by construction does not catch.

Attribution: **Lakos** (replication vs hierarchical reuse — why it is architecturally bad) + **GitClear** (in 2024 copy-paste ×8, overtook refactoring for the first time — empirical motivation). No "matter of taste".

## Context

archcheck currently flags forbidden graph edges. A duplicate is a **missing** edge: module B rewrote what already exists in A instead of depending on A. This is a separate category, covered neither by SF.*, nor by Lakos.{Cycles,GodHeader,ChainLength}, nor by Martin. And it is the #1 measured AI-drift signal — it directly reinforces the tool's AI-guardrail positioning (see #033, [docs/research/constraint_decay.md](../../docs/research/constraint_decay.md)).

**Off by default**, opt-in via `--duplication` / `defaults.duplication: true`. NOT in the v0.1 zero-config defaults.

## What the spike already established (do NOT rediscover)

The starter spike lives in [experiments/clone_detector/clone_spike.cpp](../../experiments/clone_detector/clone_spike.cpp) — two in-memory TUs, a cross-module Type-2 clone detected.

1. **Mechanism resolved.** The built-in `alpha.clone.CloneChecker` is per-TU and useless — most clones are cross-TU. We drive the underlying `clang::CloneDetector` (`clang/Analysis/CloneDetection.h`) ourselves: each TU is a separate `ASTUnit`, **keep all ASTUnits alive** (StmtSequence references into their `ASTContext`); feed every function body via `analyzeCodeBody(FD)` into one shared detector; `findClones` with a constraint pipeline.
2. **Cross-TU detection confirmed** by the spike (Type-2 with consistent renaming, members in different files).
3. **Constraint pipeline (clang 18):**
   `RecursiveCloneTypeIIHashConstraint`, `MinComplexityConstraint(N)`, `MinGroupSizeConstraint(2)`, `RecursiveCloneTypeIIVerifyConstraint`, `OnlyLargestCloneConstraint`.
   **Do not use** the "suspicious"/variable-pattern branch for copy-paste hints — its suggestions are ~50% accurate (FIXME in the clang source). `MatchingVariablePatternConstraint` does not crash across different `ASTContext`s (verified), but it does NOT cut noise — noise is cut by `MinComplexity`.
4. **The main FP risk is granularity.** The detector reports clones at every `StmtSequence` level, not just functions. Small fragments (a bare `for`) produce false cross-file "clones". In the spike: `MinComplexity=5` → clone + noise; `MinComplexity=15` → clone only. Cured by `MinComplexity` + a "function-level clones only" policy.
5. **Memory is the main UNMEASURED risk.** We hold all ASTs at once. Instant on a toy input; unknown on large repos. That is exactly Stage 0.
6. **Build:** link against `libclang-cpp` + `libLLVM` (flags in the README next to the spike).

## Non-goals (YAGNI)

- Type-3 (gapped) and Type-4 (semantic) clones.
- A custom token/winnowing backend for the fast path without `compile_commands.json` (possible v2).
- GUI/visualization.
- Inclusion in the zero-config defaults.
- Variable-pattern "suspicious copy-paste" hints.

## Execution plan

### Stage 0 — Measure perf/memory (DECISION GATE)

**Do this first.** Do not start Stage 2 without a green gate.

- [ ] Harness: take `experiments/clone_detector/clone_spike.cpp`, replace the string sources with a real walk over `compile_commands.json` (via `clang::tooling::ClangTool` / `buildASTs`, or `buildASTFromCodeWithArgs` over the file list from the DB). Keep the detector/pipeline/report.
- [ ] Run in increasing order: (1) `fmt` — baseline, (2) `spdlog` — medium, (3) `folly` or `llvm-project` — stress.
- [ ] Record in a table: `repo | LOC | #TU | peak RSS | parse time | findClones time | #cross-component function clones`.
- [ ] Run `findClones` at 2–3 values of `MinComplexity` (10 / 20 / 40) → how the number of groups changes (signal/noise).
- [ ] Record the **gate decision** in `experiments/clone_detector/PERF.md` explicitly.

**GATE (thresholds — agree with the maintainer; the defaults below are placeholders):**
- peak RSS on `spdlog` **> ~4 GB** OR `findClones` superlinear/unusable (> tens of seconds on a medium repo) → **STOP**. Whole-repo retention of the AST is rejected → Stage 1 in the heavy variant (index/fingerprint) or reconsider the feature.
- Within budget → Stage 1.

### Stage 1 — Scope decision: whole-repo vs changed-files / baseline

**Important:** the comparison corpus = the WHOLE repo always. "Analyze only the diff" is impossible — a clone is paired, the second member is in unchanged code, there is nothing to compare against. `--changed-files` is a **report filter** (report a group only if at least one member is in the changed set), not a narrowing of the analysis. `--baseline` is a **freeze** of existing duplicates.

- [ ] Based on the Stage 0 numbers, choose:
  - **whole-repo cheap** → analyze the entire corpus; add `--baseline` (consistent with archcheck's baseline model) and/or `--changed-files <list>` (PR mode).
  - **whole-repo heavy** → a persistent fingerprint index (hashes of function bodies), cached in CI by base commit; rehash only changed TUs. The heavy path — only if Stage 0 forced it.
- [ ] Reference for the diff/incremental CI model: **Ericsson CodeChecker** (re-analyze only changed files, diff "new vs fixed"). Copy the model, not the code.
- [ ] **Deliverable:** a ≈1-page design note (`docs/dev/design_duplication_scope.md`) with a recommendation of one variant and a justification by the Stage 0 numbers. Agree with the maintainer BEFORE integration.

### Stage 2 — Integration as an optional pass (only if the gate is green)

**Placement (OCP):**
- [ ] A new `DuplicationRule` class under `src/rules/duplication/`, implementing the semantic-rule interface (`ISemanticRule` from #042). Registration via a static table. Adding = a new file; existing code is not edited.

**Reuse (DRY):**
- [ ] `scan/clang_scanner` — AST (libclang backend only; needs `compile_commands.json`).
- [ ] `graph/component_graph` — map the `SourceLocation` of each clone-group member → component.
- [ ] The existing `report/{text,json,sarif}_reporter`.

**Logic:**
- [ ] Report **only cross-component** groups; wording in the graph vocabulary:
  `"missing reuse edge: components A and B share fragment → candidate shared component"`.
- [ ] **Granularity policy:** function-level clones; filter out sub-statement noise (the `MinComplexity` threshold + discarding sub-sequences nested in a larger group).
- [ ] **The duplication-ratio metric** — into the report.

**Config and flags:**
- [ ] **Off by default.** Enabling: CLI `--duplication` OR `defaults.duplication: true` in `.archcheck.yml`. NOT in the zero-config defaults.
- [ ] `thresholds.duplication_min_complexity` (tunable, pick the default from Stage 0, guideline from the spike ≥15).
- [ ] Exclude paths for generated/third_party code — **mandatory** (protobuf/grpc/generated are full of legitimate duplication).
- [ ] Respect `// archcheck: allow`.
- [ ] Contributes to exit code `1` only when the pass is enabled.

**Tests (integration fixture with a known cross-component clone):**
- [ ] Reuse the two-module fixture from the spike.
- [ ] Assert: the planted clone is found.
- [ ] Assert: the control non-clone function is NOT grouped.
- [ ] Assert: sub-statement noise is filtered out at the chosen `MinComplexity`.

**Documentation:**
- [ ] An entry in the rules doc with attribution (**Lakos** physical design + **GitClear** empirics).
- [ ] Explicitly: the pass is opt-in and requires `compile_commands.json` (AST backend).

## Acceptance criteria

- [ ] Stage 0: `experiments/clone_detector/PERF.md` with a table committed, the gate decision recorded.
- [ ] Stage 1: `docs/dev/design_duplication_scope.md` agreed.
- [ ] The pass is behind a flag, **off by default**.
- [ ] Finds the planted cross-component clone in the fixture; no false group from the control function; sub-statement noise filtered out.
- [ ] Generated/third_party excluded; `// archcheck: allow` respected.
- [ ] Only cross-component groups; report in the graph vocabulary; duplication ratio in the report; json/sarif work.
- [ ] No new runtime dependencies beyond libclang/LLVM.
- [ ] One rule class, existing code untouched (OCP).

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Wait for #043 to go green (the shared fate of the libclang backend). If #043 cancels the feature — reassess this one too.
2. After #042 phase 1 (the `ISemanticRule` + `SemanticContext` skeleton + loading `compile_commands.json`) — start Stage 0 on that infra.
3. If Stage 0 is a red gate, discuss the fingerprint index with the maintainer BEFORE Stage 1.

## Key decisions

| Decision | Reason |
|---------|---------|
| Drive `clang::CloneDetector` ourselves, do not use `alpha.clone.CloneChecker` | the built-in check is per-TU, our case is cross-TU |
| Without `MatchingVariablePatternConstraint` | suggestions ~50% accuracy (FIXME in clang); noise is cut by `MinComplexity` anyway |
| Only function-level clones in the report | sub-statement matches produce false cross-file groups |
| Off by default | an expensive pass (libclang + AST retention), AI-drift audit is opt-in |
| Lakos + GitClear attribution | project rule: every default rule carries attribution; here — authority + empirics |
| Stage 0 gate is MANDATORY | memory on the whole-repo AST is the main unmeasured risk; without numbers we do not proceed to Stage 2 |

## Alternatives (rejected / deferred)

- **`alpha.clone.CloneChecker`** — per-TU, useless for the cross-TU case.
- **Type-3/Type-4** (gapped / semantic) — out of scope; accuracy drops sharply, the FP cost outweighs the benefit.
- **Token/winnowing backend** for the fast path without `compile_commands.json` — deferred to a possible v2; the AST backend is enough for an opt-in PoC.
- **"Analyze only the diff"** — impossible: a clone is paired, the second member is in unchanged code. See the Stage 1 explanation.
- **Include in the v0.1 zero-config defaults** — violates "zero-config first": expensive, requires `compile_commands.json`, risk of noise on legacy repos.
- **Variable-pattern "suspicious copy-paste" hints** — a separate task from cross-TU clone detection, ~50% accuracy.

## Changed files

| File | Change |
|------|-----------|
| `experiments/clone_detector/clone_spike.cpp` | starter — already in place, proves cross-TU detection (Stage 0 builds on it) |
| `experiments/clone_detector/CMakeLists.txt` | building the spike against libclang-cpp + libLLVM |
| `experiments/clone_detector/PERF.md` | Stage 0 result (table + gate decision) |
| `docs/dev/design_duplication_scope.md` | Stage 1 result (whole-repo vs index, ≈1 p.) |
| `include/archcheck/rules/duplication/duplication_rule.h` + `src/rules/duplication/duplication_rule.cpp` | Stage 2 — `ISemanticRule` impl |
| `src/rules/rule_set.cpp` | registration (gated behind `--duplication` / `defaults.duplication`) |
| CLI (entry point) | the `--duplication` flag; YAML — `defaults.duplication`, `thresholds.duplication_min_complexity`, exclude paths |
| `fixtures/duplication/cross_component_clone/` | two-module fixture (replicates the spike scheme as real files) |
| `tests/...` | integration tests on the fixture |
| `docs/rules/duplication.md` | rule entry, Lakos + GitClear attribution |

## Fixtures

- [ ] `fixtures/duplication/cross_component_clone/` — two modules, one Type-2 clone + one control non-clone function; expected report = 1 cross-component group.
- [ ] `fixtures/duplication/sub_statement_noise/` — a pair of functions with the same bare `for` loop; at `MinComplexity ≥ default` — 0 groups.
- [ ] `fixtures/duplication/generated_excluded/` — generated files under an exclude pattern; 0 groups.

## Pointers

- **Start:** `experiments/clone_detector/clone_spike.cpp` — proves cross-TU detection + the constraint pipeline and contains a sweep over `MinComplexity`. For Stage 0, replace the string sources with a walk over `compile_commands.json`.
- **API:** `clang/Analysis/CloneDetection.h`. Key: `CloneDetector::analyzeCodeBody(const Decl*)`, `CloneDetector::findClones(result, constraints...)`, `StmtSequence::{getBeginLoc,getContainingDecl}`.
- **Perf reference:** Ericsson CodeChecker (incremental CI model).
- **Empirics:** GitClear 2024 — copy-paste ×8, overtook refactoring for the first time (see [docs/research/constraint_decay.md](../../docs/research/constraint_decay.md), [docs/research/ai_drift_runlog.md](../../docs/research/ai_drift_runlog.md)).
