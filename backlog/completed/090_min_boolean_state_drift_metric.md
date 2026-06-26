# [RULES] boolean_state_accumulation — drift metric (deferred v0.3+)

**Creation date:** 2026-06-07
**Start date:** 2026-06-24
**Completion date:** 2026-06-25
**Status:** done
**Target release:** v0.3+ (when there is demand)
**Module:** RULES / DRIFT
**Priority:** minor
**Blocks:** —
**Blocked by:** —
**Related:** #089 (research), #135 (metric validated — absorbed by this task), #136 (parser fix), #086/#087 (drift family), #119 (correlation — the column's consumer)

## OUTCOME (2026-06-25) — how it works

`DRIFT.BOOL_FIELD_ACCRETION` — an advisory diff rule (`src/scan/bool_field_drift.{h,cpp}`, factory
`makeDriftRuleSet`). On `--diff` it compares the old/new slice of each changed authored file:
for each struct existing in both slices (by `kStructRe`), it counts the net increase
of depth-0 bool fields `Σ max(0, count_after − count_before)`; rename/replace/reformat → 0.
Parser — a port of `perstruct_drift.struct_fields` with the #136 fix (`neutralizeBraces` blanks `{`/`}`
in string/char literals and comments before counting braces). The vendored/test/generated filter
is inherited for free via `SourceSnapshot.authored` (`file_classification.h`) — there's no Python filter duplicate.
Advisory-only, never gates, no knob (YAGNI).

**Validation:** unit+integration fixtures (pass/fail_accretion); oracle C++==Python 11/11;
corpus run of 520,177 commits (100%, 35.8 h) → columns `n_bool_field`/`n_bool_struct`,
**10,735** non-zero commits (2.07%), 17,510 bools in 13,315 structs; FP check 22/22 TP;
cross-check with the ctags run — 15 core columns 100% (the bool rule perturbs nothing), the only
mover `n_other` = an artifact of rebuilding the binary under a live ctags run; dogfood on archcheck itself
0 noise (statics 0 + 60 of its own commits 0, the silence verified against ground truth). The metric is **neutral**
(accretion ≠ defect); the interpretation comes from the #119 correlation, not the raw count.

## REOPENED 2026-06-24 — implementing as an advisory diff rule `DRIFT.BOOL_FIELD_*`

User decision: instead of the research sidecar #135 (Python, reinvents scan+filter) — a **native
archcheck rule**. The vendored/generated/test filter is inherited for free via `SourceSnapshot.authored`
(`file_classification.h`); no Python filter duplicate.

**The metric is NOT the old naming-detect below (reverted in `4268a39`, 78% noise), but the one validated in #135:**
per-commit **net increase in the number of depth-0 bool fields in a struct that existed in the parent**
(`Σ max(0, count_after − count_before)` over structs that exist in both the old and new version of the file).
Rename/replace/reformat → 0. Parser — a port of `perstruct_drift.struct_fields` WITH THE #136 FIX (strip
literals/comments before counting braces).

**Template — `DRIFT.LOCAL_COMPLEXITY`** (`src/scan/local_complexity_drift.{h,cpp}`): verified the interface
`compareX(old,new,file)` + `detectXDrift(oldSnap,newSnap,changedFiles)`; the filter via `SnapshotFile.authored`;
the wiring in `src/cli/diff_command.cpp` (`DiffAdvisories`/`collectX`/`flattenAdvisories`/print); JSON via
`writeViolations` → `advisory.violations[]`. **The validation oracle is the Python sidecar #135** (C++ == Python).

### Commit plan (≤50 lines/commit, ≤2 files, fixtures mandatory)
- [x] C1: `include/archcheck/scan/bool_field_drift.h` — interface. Commit `e778f13`.
- [x] C2: `src/scan/bool_field_drift.cpp` — parser (port + full #136 fix `neutralizeBraces`) + `compareBoolFields` + 8 unit tests. `e778f13`.
- [x] C3: `detectBoolFieldDrift` (`findFile`/`authored`) + CMake. `e778f13`.
- [x] C4: wiring in `diff_command.cpp` + JSON. End-to-end: FlashCpp +3, ovn +1, vendored SDL→0, the KsanaLLM bug absent. `e778f13`.
- [x] C5: fixtures `fixtures/bool_field_drift/{pass,fail_accretion}` + integration test (25 assert / 11 cases).
- [x] C6: cross-check C++ == Python oracle on a sample — **parity is perfect** (11/11 by file+struct+delta, 0 discrepancies); the filter is more precise than regex (keeps its own `lib/Engine`, drops the real vendored).
- [x] C7: CHANGELOG + GLOSSARY. **Threshold decision: fire on delta≥1, advisory-only, no knob** (YAGNI; advisory tolerates +1 like SATD-on-every-TODO; the corpus column needs a raw count). Dogfood + full run — below.

### Remaining — all closed
- [x] Dogfood: static self-check `archcheck src include tests` → 0 violations; `--diff` over the last 60 of my own commits → **0** firings of `DRIFT.BOOL_FIELD_ACCRETION`. Skeptic-check of the silence: 84 added `bool` lines in history — these are function locals of parsers + fields of **new** files (#129 `source_snapshot.h` added with status `A`, greenfield); not a single accretion into an existing struct. The rule is knowingly live in this call (10,735 firings on the corpus + 22/22 FP check via the same `--diff --diff-mode=memory`), so 0 on archcheck is real silence, not a disabled rule. It doesn't make noise on its own code. ✓
- [x] Full corpus run with the native rule → column `n_bool_field` in `results_full.boolrule.jsonl`: rebuilt the release binary, added a bucket to `run_worklist.py categorize()`, ran **520,177 commits (100%, 35.8 h)**. Result: **10,735** commits with `n_bool_field>0` (2.07%), 17,510 bools in 13,315 structs. FP check 22/22 TP. Cross-check with the ctags run: 15 core columns 100% on "both-ok", the only mover `n_other` — an artifact of rebuilding the binary under a live ctags run (bool violations partly leaked into `else→n_other` of the old categorize), doesn't affect the bool dataset. The dataset is ready for the #119 correlation.

---

### (OBSOLETE original plan — naming-detect, REVERTED in `4268a39`, for the record)

> **Rethought following research #089.** The original plan "static rule `implicit_state_machine_growth` (5+ bool + state names)" CANCELED: the empirics on 790 repos showed that naming-detect is useless (the only flag — an FP), and the static counter — 78% noise. The working signal is only **per-struct accumulation over git history**. See the design: `docs/research/boolean_state_metric_design.md`.

## Goal

Implement (if there is a request) the drift metric `boolean_state_accumulation`: a struct accumulated bool fields across ≥4 different commits, the fields are interdependent → a growing implicit FSM. NOT a static linter; a history metric alongside #086/#087.

## Why deferred

- **NOT because of #042.** The metric is over git history; an AST per commit is unrealistic (×1350, old commits don't build). Gates 1-3 = git blame + regex (fast backend). Gate 4 (interdependence) = a cheap regex proxy over group assignment on the current slice; #042 would give only a marginal boost.
- Requires diff/history (mode `--diff`, not single-shot scan).
- **YAGNI** — none of the users asked for it; for archcheck it's a borderline candidate (risk of breaking "not a linter"). That is the real blocker — demand, not technique.
- A prototype already exists: `experiments/boolean_state/perstruct_drift.py` (0% gross FP on verification).

## Plan (when/if doing it) — from metric_design.md

- [ ] Gate 1: depth-0 parsing of fields (without signatures/locals).
- [ ] Gate 2: per-struct attribution + git blame.
- [ ] Gate 3: history-completeness check (shallow → lower-bound).
- [ ] Gate 4: interdependence — a regex proxy over group assignment on the current slice (config-bag/bloat vs implicit FSM); #042 — optional boost later.
- [ ] Thresholds: nfields≥5, drift_commits≥4.

## Context

Research #089 proved:
- Boolean-state growth — a real drift signal (28 candidates in a 50-repo corpus sample, 16% prevalence)
- Verdict: **YES**, it can be implemented with ~72% precision on regex heuristics without a semantic backend
- Authority: "Make Illegal States Unrepresentable" (Martin) + State Pattern (GoF)

Rule 1 parameters:
- Threshold: 5+ bool fields + 3+ state-pattern names (60% ratio)
- Implementation complexity: L (Low) — 100-150 lines
- FP mitigation: allowlist (*Options, *Config, Chord*, etc.)

## Execution plan

### 1. Integrate extractor into src/
- [ ] Port `experiments/boolean_state/extractor.py` logic into `src/rules/` (C++20 native)
- [ ] Create `src/rules/boolean_state_detector.h` + `.cpp`
- [ ] Register in `rule_set.h`

### 2. Implement IRule class
- [ ] Inherit from IRule, implement `check(const DependencyGraph&, const Config&) -> vector<Violation>`
- [ ] Fields: min_bool_fields (configurable, default 5), state_pattern_ratio (default 0.6)
- [ ] Struct extraction (scan header files for `struct`/`class` with bool members)
- [ ] Naming heuristics (state-pattern vs config-pattern matching)

### 3. State-pattern dictionary
- [ ] State names: {started, running, paused, failed, completed, cancelled, ready, active, enabled, loaded, connected, initialized}
- [ ] Exclude patterns: {*Options, *Config, *Settings, *Flags, *Parameters, Chord*, Flat*}
- [ ] Config names: {enable_, use_, allow_, verbose, debug, trace}
- [ ] Make configurable via YAML `implicit_state_machine_growth:` block

### 4. Fixtures
- [ ] `fixtures/implicit_state_machine_growth/pass/valid_state_machine.h` — 5+ state-bools → flag (VIOLATION)
- [ ] `fixtures/implicit_state_machine_growth/fail_under_threshold.h` — 4 bools → OK
- [ ] `fixtures/implicit_state_machine_growth/fail_config_named.h` — 5 bools, all config-named → OK
- [ ] `fixtures/implicit_state_machine_growth/fail_excluded.h` — ChordCombo-style bitmask → OK (excluded)

### 5. Testing
- [ ] Unit tests (10 cases: pass/fail variants)
- [ ] Corpus validation: Run on 50-repo sample, verify precision ≥70%
- [ ] Dogfood: archcheck itself must pass (verified in #089)
- [ ] CI check with code_review

### 6. Documentation
- [ ] docs/rules/implicit_state_machine_growth.md
- [ ] Add to CHANGELOG.md (v0.2 section)
- [ ] CLI help text

## Done

> **Historical section — the code below is NOT in the repo.** The described implementation existed and was
> **deliberately reverted** by commit `4268a39` "revert(rules): remove implicit_state_machine_growth
> from the main archcheck" after a rethink following #089 (static naming-detect —
> 78% noise; the working signal is only history-based, see the file header). The morning backlog review
> 2026-06-11 mistakenly judged the section a hallucination — no, it's the trace of a real revert cycle.
> The plan above (gates 1–4) is the current replacement; the "Fixtures" checkboxes at the bottom also relate
> to the reverted version.

- **Rule implementation (C++20):** `src/rules/implicit_state_machine_growth.h/cpp` — struct extraction, bool-field counting, state-pattern matching
- **Threshold logic:** min_bool_fields=5, state_pattern_ratio=0.6 (60%) — tuned from corpus study (#089)
- **Heuristics:** state-pattern matching (started/running/paused/failed/active/ready/connected), config-pattern exclusion (enable_*/use_*/verbose/debug), exclude-list (*Options/*Config/Chord*/FlatC*)
- **Fixtures:** 4 test cases (pass + fail variants) → all correctly classified
- **Unit tests:** 5 test cases (Catch2) → all passed ✓
- **Integration:** registered in rule_set, added to CMakeLists, dogfood check pending
- **Documentation:** rule message with field names, ratios, suggestion to use State Pattern

## In progress

- (empty)

## Next steps

1. Dogfood: verify archcheck itself passes the rule (should pass, no 5+ bool structs found)
2. v0.3: implement Rules 2-3 (semantic backend #042 dependent)
3. Baseline integration: track boolean-field growth over time as drift metric
4. User feedback: gather ground truth from real projects

## Key decisions

| Decision | Reason |
|---------|---------|
| C++20 native (not Python wrapper) | Consistent with codebase style; avoid runtime Python dependency |
| Threshold: 5+ bools | From corpus study: balances recall (~80%) vs FP (~28%) |
| Exclude patterns allowlist | Handle known FP cases (ChordCombo, FlatCOptions, etc.) |
| State-pattern naming required | Not just boolean count; must match known state names |
| YAML configurable | Users can tune thresholds per project, add/remove patterns |

## Changed files

| File | Change |
|------|-----------|
| `src/rules/boolean_state_detector.h` | new (IRule subclass) |
| `src/rules/boolean_state_detector.cpp` | new (implementation) |
| `include/archcheck/rules/rule_set.h` | modify (register rule) |
| `fixtures/implicit_state_machine_growth/` | new (pass/ + fail_*/) |
| `docs/rules/implicit_state_machine_growth.md` | new (user docs) |
| `CHANGELOG.md` | update (v0.2 release notes) |

## Fixtures

- [ ] `fixtures/implicit_state_machine_growth/pass/state_machine.h` (5 bools, state-named)
- [ ] `fixtures/implicit_state_machine_growth/fail_under_threshold.h` (4 bools)
- [ ] `fixtures/implicit_state_machine_growth/fail_config_flags.h` (5 bools, config-named)
- [ ] `fixtures/implicit_state_machine_growth/fail_bitmask.h` (bitmask, excluded)
