# [SCAN] Clone-detector FP reduction — classification gaps + cosmetic data-table guard

**Created:** 2026-06-29
**Status:** wip — Parts A+C DONE (commit `5277cd2`); Parts B+D still open
**Module:** SCAN / duplication (#056/#070), classification (#129)

## Done 2026-06-29 — Parts A + C (commit `5277cd2`)

Drafted by Haiku, reviewed + fixed by Opus. Verified: 610 tests, dogfood 0, lizard clean,
corpus acceptance (chrxh_alien `*Tests.cpp` pairs 7→0; djeada data-table pairs 518→477).

- **Part A** shipped: `isTestBasename`/`isTestDirName` now match CamelCase `XxxTests`/`XxxTest`
  (capital-T + lowercase-before guard) and the `.test.`/`.spec.` infix.
- **Part C** shipped: `phase10DataTableClassifier` is a real DROP (both diversity<0.30 AND
  cloneType∈{LITERAL,MIXED}), togglable via `enableDataTableDrop`.

**Review catches (the value — see JOURNEY 2026-06-29):**
1. The CamelCase size guards were off by one (`>=5`/`>=4` then read index `size-6`/`size-5`)
   → out-of-bounds read on a bare `Tests`/`Test` segment. Fixed to `>=6`/`>=5`.
2. The data-table guard fires ONLY on a pair that ALREADY clears the joint floor with both
   diversity<0.30 AND LITERAL/MIXED. An all-different-literals palette (every line differs)
   is killed UPSTREAM by the floor (low line overlap), never reaching the guard — the draft's
   fixture proved nothing. The real target is the djeada signature: a table that is byte-
   identical except one line (`default:`) → high line overlap, LITERAL, low diversity.
3. Measurement trap recorded: a stale (un-relinked) binary made the guard look like a no-op
   (518==518). The lib-level probe is authoritative (1266→1223). Always re-link before A/B.

---

**Origin:** eyeball triage of the CURRENT detector's actual fires (not corpus labels —
those proved unreliable: several round-2 "FP" are real copy-paste). 82 fires across 11 repos
read by hand; raw dumps were in scratch (`fphunt.py` triage). Sample precision ≈ 75-80 %;
the FP mass clusters in the classes below.
**Related:** #070 (FP guards), #129 (AuthoredScope test/vendor/generated gate), #131 (Group 3
recall fix P0.6b — added the run-path that widened the coincidental class in Part D),
[docs/dev/duplication_decision_path.md](../../docs/dev/duplication_decision_path.md) (read FIRST).

---

## Goal

Cut the duplication detector's false positives at their real sources, found by reading the
detector's own output. Four root causes, two of them **mechanical and Haiku-ready** (Parts A, C),
two that **need measurement/design first — NOT for Haiku** (Parts B, D).

## Evidence — the FP classes (with verified causes)

| Class | Example (repo : file) | Verified cause |
|---|---|---|
| **Unit-test / fuzzer boilerplate** | chrxh_alien `source/EngineTests/ObjectConnectionTests.cpp` (all 7 sampled fires were test bodies); AztecProtocol `alu_trace.test.cpp`, `bigfield.fuzzer.hpp` | test files NOT excluded — see Part A. `--duplication` filters via `AuthoredScope::fromFiles` (`project_files.cpp:160`, #129), which calls `isTestBasename`/`isTestDirName`, but those miss camelCase + infix naming. |
| **Generated code** | cschladetsch_CppKAI `SimpleCalculator.proxy.h` / `.agent.h` RPC stubs (Add/Subtract/Multiply near-identical) | `.proxy.h`/`.agent.h` not in `isGeneratedPath` (`file_classification.h`). Part B (design). |
| **Data-table / palette** | djeada `skeleton_archer.cpp:17` ↔ `swordsman.cpp:18` (`case N: return {r,g,b};` color tables, ×3); `scatter_rules.h` (enum→multiplier switch) | `phase10DataTableClassifier` only **down-weights** (×0.85) and runs AFTER the joint floor → no suppression. Part C. |
| **Vendored single-header** | NamecoinGithub_LLL-TAO `json.h` (nlohmann, char-dispatch state machine) | a vendored single header not under a vendored DIR isn't excluded (same gap as #131 Group 1). Part B (design). |
| **Coincidental / shared-skeleton** | graphillion_kyotodd BDD iterators (`enum Phase{...}; struct Frame{...}; stack.reserve(64)` shared across ~7 iterators, line-overlap 0.20-0.35); fuddlesworth gap setters; djeada `terrain.cpp` river-vs-bridge | low line-overlap pairs sharing a scaffold but with divergent bodies. **Admitted by the #131 P0.6b run-path** (≥6 shared lines + diversity≥0.30 + weighted≥0.60). Part D (measurement). |

---

# PART A — test-file recognition gap (HAIKU-READY)

**Resolved facts (checked 2026-06-29):**
- `isTestBasename` (`include/archcheck/scan/file_classification.h:441`) matches only: stem starts
  with `test_`/`test-`, OR stem ends with one of `_test _tests _spec -test -tests -spec`
  (separator required). It is **case-insensitive** (operates on `toLowerAscii(filename)`).
- `kTestDirNames` (`file_classification.h:424`) = `{test, tests, tst, testutil, testutils,
  unittest, unittests}`; `isTestDirName` normalizes the segment via `normalizeDirSegment` (lowercases,
  strips separators) then exact-matches the set.
- **Gaps that leak (confirmed by the triage):**
  - `ObjectConnectionTests.cpp` → stem `objectconnectiontests` ends with `tests` but **no separator** → not matched.
  - `alu_trace.test.cpp` → stem `alu_trace.test` ends with `.test` not `_test` → not matched.
  - dir `EngineTests/` → normalizes to `enginetests` ≠ any `kTestDirNames` entry → not matched.

**The fix — extend recognition, with a hard guard against false matches.**

### A.1 `isTestBasename` — add two patterns

Add to `isTestBasename`, AFTER the existing suffix loop, BEFORE `return false`:

1. **CamelCase suffix** — match when the **original** (not lowercased) `filename` stem ends with
   `Test` or `Tests` AND the character before that suffix is a lowercase ascii letter (a CamelCase
   boundary). This requires checking the raw `filename` (the function currently only keeps the
   lowercased `name`); add a parallel raw-stem check. Capital `T` is the discriminator.
2. **Dotted infix** — match when the lowercased `filename` contains `.test.` or `.spec.`
   (e.g. `alu_trace.test.cpp`, `foo.spec.cc`).

**CRITICAL negative cases (must NOT be classified as test — guard 1.5 of the Haiku guide):**
`latest.cpp`, `contest.cpp`, `fastest.cpp`, `attestation.cpp`, `greatest.cpp`, `protest.cpp`.
All contain the substring `test` but: lowercase `t` (not CamelCase `Test`) and no `.test.` infix.
The capital-`T` requirement is what excludes them — do NOT lowercase before the CamelCase check.

### A.2 `isTestDirName` — add CamelCase dir suffix

A dir segment whose **raw** form ends with `Test` or `Tests` preceded by a lowercase letter
(`EngineTests`, `UnitTests`, `EngineTest`) is a test dir. Same capital-`T` + lowercase-before guard.
`normalizeDirSegment` lowercases, so this check needs the raw segment — add it in `isTestDirName`
before normalization, mirroring A.1's logic. Negative: `manifest/`, `greatest/` must NOT match.

### A.3 Fixtures (mandatory — acceptance bar)

- `fixtures/file_classification/` (or extend the existing classification test
  `tests/unit/scan/` — grep `isTestBasename` in tests/ for the live test file, checked 2026-06-29):
  assert TRUE for `ObjectConnectionTests.cpp`, `FooTest.cpp`, `alu_trace.test.cpp`, `bar.spec.cc`;
  assert FALSE for every negative case in A.1; assert dir TRUE for `EngineTests`, FALSE for `manifest`.
- One **duplication** fixture: `fixtures/duplication/test_boilerplate/` — two files
  `WidgetTests.cpp` + `GadgetTests.cpp` each with an identical arrange-act-assert block; assert
  `scanForDuplication` over the directory (through `collectNonVendoredSources`) reports **0 pairs**
  between them (they are excluded as tests). Mirror the existing duplication fixture style.

### A.4 Acceptance (numbers)

- All A.3 unit assertions pass.
- `./build/debug/src/archcheck --duplication ~/oss/chrxh_alien/source` no longer reports
  any pair whose BOTH sides are `*Tests.cpp` (before this task: ≥7 such pairs in the sample).
- Existing suite stays green; **dogfood**: `./build/debug/src/archcheck src include tests` = 0 violations
  (archcheck's own `tests/` are already excluded by `tests/` dir; this change must not regress that).

### A.5 No expectation flips

This change only EXCLUDES more files. No existing test encodes "a `*Tests.cpp` clone must be
reported", so no expectation needs inverting (guard 1.5). If grep finds one, STOP and report it —
do not delete it silently.

---

# PART C — make the data-table guard actually suppress (HAIKU-READY, tight spec)

**Resolved fact (checked 2026-06-29):** `phase10DataTableClassifier`
(`src/scan/duplication/duplication_scanner.cpp:186`) multiplies `p.weighted *= 0.85` when both
fragments have `diversity < 0.30`, and runs in `applyCandidateFilters` AFTER
`phase8JointTokenOrderFloor` with **no subsequent threshold re-check** — so it changes only the
reported weight, never membership. The palette/enum-table FPs survive.

**The fix:** turn it into a real DROP. A pair is a data table when **both** fragments are
`diversity < 0.30` **AND** the clone type is `LITERAL` or `MIXED` (set by `phaseClassifyCloneType`
— but that runs last; instead test it inline here via `cloneType(frags[p.a], frags[p.b])`, or move
the data-table phase to run after classification). Drop such pairs.

- **Why both conditions:** `diversity<0.30` alone could catch a real low-diversity logic copy;
  requiring LITERAL/MIXED ties it to "the difference between the two is only literals" — the
  signature of a `case N: return {lit,...}` palette or an enum→constant map.
- **Parameters:** `diversity < 0.30` (existing), type ∈ {LITERAL, MIXED}. Add a `ScannerOptions`
  bool `enableDataTableDrop = true` so it is togglable (mirror `enableP1Guards`).

**Control cases with expected outcomes (guard 1.4):**
- djeada `skeleton_archer.cpp:17` ↔ `swordsman.cpp:18` (LITERAL, both low-diversity) → **dropped (NO finding)**.
- djeada `scatter_rules.h` enum→multiplier switch (MIXED, low-diversity) → **dropped**.
- A real STRUCTURAL copy with diversity≥0.30 (e.g. ThemisDB `mysql_importer`↔`sqlite_importer`,
  LITERAL but diversity high) → **still reported** (verify diversity ≥ 0.30 by hand first; if it is
  LITERAL+low-diversity, it is correctly a borderline drop — record the number, do not assume).

**Fixtures:** `fixtures/duplication/data_table/pass/` (a real logic copy that must still fire) and
`fixtures/duplication/data_table/fail_palette/` (two `case N: return {lit,lit,lit};` switches that
must NOT fire). Acceptance: pass-fixture reports the pair, fail-fixture reports 0.

**Expectation flips:** check `duplication_fp_guards_test.cpp` / `duplication_synthetic_fp_test.cpp`
for any existing data-table case asserting a down-weighted-but-present pair; if found, its
expectation changes from "present with reduced weight" to "absent" — update it and note WHY in the
commit (the down-weight was a no-op; suppression is the intended behavior).

---

# PART B — generated / vendored single-header (NOT HAIKU — needs design)

Do **not** hand to Haiku. These need a human/senior decision because naming-based exclusion is
risky (a `.proxy.h` could be hand-written; a vendored single-header has no path signal):
- **Generated RPC stubs** (`.agent.h`, `.proxy.h`, `.fuzzer.hpp`): decide between a naming list
  vs a content-banner check (preferred — `isGeneratedPath` already reads banners for some types).
- **Vendored single-header** (nlohmann `json.h`, etc.): same open gap as #131 Group 1
  (whole-repo-vendored / bundled headers). Solve together, not piecemeal.

# PART D — coincidental / shared-skeleton (NOT HAIKU — needs measurement)

The #131 P0.6b run-path (`phase8JointTokenOrderFloor`, run path: `sharedLines ≥ 6 &&
diversity ≥ 0.30 && weighted ≥ jointRunWeightedThreshold`) admits pairs that share a scaffold
(graphillion `Frame`/`stack` iterator skeleton; setters; declaration runs) but have line-overlap
0.20-0.35 and divergent bodies. Candidate levers (each needs a corpus recall↔precision measurement
via `experiments/corpus_remeasure_131/group3_*.py` — that harness reports BOTH metrics):
1. require the shared line run to be **contiguous** (a real block), not scattered — `sharedLines`
   is currently an order-LCS length over `normLineSeq`; add a longest-contiguous-run measure and
   gate on it.
2. raise `jointRunWeightedThreshold` (0.60 → 0.65/0.70) — trades int64/uint64-class recall.
3. require a rare anchor on the run path (the #131 `jointMinSharedRare` knob, currently 0) with a
   calibrated `jointAnchorDfCap` — #131 showed df≤4 too strict, df≤12 leaked; needs a sweep.

Decide D only with the curve in hand. Do not tune blind.

---

## Working order

1. **Part A first** (highest FP mass, lowest risk, fully mechanical).
2. **Part C** (tight spec, fixtures gate it).
3. Re-run the eyeball triage (`fphunt.py` style) on 4-5 repos to confirm the test/data-table mass
   dropped and nothing real regressed.
4. Parts B, D — separate tasks once a human picks the approach / a sweep is run.

## Self-check (mandatory)

- Every fixture has BOTH a pass and a fail case (project rule).
- The CamelCase test rule's negative cases (`latest`, `attestation`, …) are unit-asserted — this is
  the one trap in Part A.
- After Parts A+C, dogfood `archcheck src include tests` = 0; full suite green; lizard clean
  (`lizard --CCN 15 --length 30 --arguments 5 --warnings_only` on touched files).
