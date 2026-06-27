# [SCAN][CONFIG] Consolidate file-classification constants into one parameterizable home

**Created:** 2026-06-27
**Started:** —
**Status:** new
**Module:** SCAN / CONFIG / RULES
**Priority:** major
**Complexity:** L (touches ~10 consumers + authored_scope; constexpr→runtime delivery is a design decision, not a mechanical move)
**Blocks:** —
**Blocked by:** —
**Related:** #041 (audit_hardcoded_strings — **direct ancestor**, this supersedes its decision-5 "do NOT YAML-ify patterns"), #051 (config_loader_v1), #129 (lifted isGeneratedPath into the shared header), #127 / #069 / #068 / #070 / #081 / #109 / #113 / #114 / #147 / #151 (each piled markers into file_classification.h — the surface that grew), docs/config_format.md, memory `project_config_format_v1`, `project_config_discovery_defaults`

## Goal

Collapse every **file/dir/path classification** string list into **one parameterizable home** — a single file/class that (a) holds the curated defaults in one place and (b) can be extended/overridden from a text `.archcheck.yml`. Stop the recurring pattern where a new marker is added inline in whatever rule needs it, and the same concept ("vendored dir", "generated file", "test code", "header extension") ends up defined 2-4 times with divergent contents.

The user's framing: *"нужно чтобы был один файл или класс, который потом мог бы параметризироваться из текстового ямла"* — a single classifier, defaults embedded, YAML-extendable.

## Context — why this is reopening a solved problem

#041 (`audit_hardcoded_strings`, completed 2026-06-11) already did this once: it created `include/archcheck/scan/file_classification.h` as the "single source of truth for the file/dir classification defaults" (the `// Task #041` comment is still at line 12). **It then explicitly declined to expose those patterns via YAML** (its decision-5, 2026-06-11: *"we do NOT do YAML override of extensions … adding a new top-level key without a user request violates YAGNI"*) and ordered "Do NOT touch file_classification.h".

Two things changed since, which **re-open the decision**:

1. **The surface exploded.** Between #068 and #151 the classification logic grew from extensions + a handful of excluded dirs to **14 named arrays plus an anonymous inline suffix list** — vendored dirs, vendored lib dirs, vendored stems/exact basenames, generated path markers, license-banner phrases, generated-banner markers, test dirs, test-stem suffixes, minified-content thresholds. Each landed ad-hoc as a corpus FP was fixed.
2. **It drifted out of the canonical home anyway** — exactly the failure #041 was meant to prevent. The same concepts are now re-hardcoded in `area_of.h`, `sf9_no_cycles.cpp`, `lakos_god_headers.cpp`, `include_resolver.cpp`, each with its own slightly-different list.
3. **The user is now explicitly asking** for the YAML surface — which removes the YAGNI objection that blocked #041's decision-5.

> **Supersedes #041 decision-5** (recorded here so future-me does not read this as contradicting a closed task): the "no YAML for patterns" call was correct *at the time* (no user, small surface). As of 2026-06-27 a user asked, the surface is 4× larger, and the drift it predicted has happened. This task overturns it deliberately.

## Inventory — every file/dir/path classification constant (verified 2026-06-27)

Tiers: **A** = canonical home (keep, but tidy). **B** = drift to pull in (the actual bug). **C** = borderline, decide per-item. **D** = explicitly out of scope, do NOT move.

### Tier A — the canonical home: `include/archcheck/scan/file_classification.h`

| Constant | Line | Classifies | Note |
|---|---|---|---|
| `kProjectExtensions` (12) | 24-26 | source+header extensions scanned | |
| `kHeaderExtensions` (8) | 29-31 | header subset (SF.7/8 apply) | |
| `kExcludedDirNames` (6) | 35-37 | dirs skipped in traversal | `.git build .cache .idea .vscode out` |
| `kCmakeBuildPrefix` | 38 | `cmake-build-*` dir prefix | |
| `kVendoredDirNames` (15) | 115-118 | vendored container dirs (normalized) | |
| `kVendoredLibDirs` (14) | 126-129 | in-tree bundled libs (jpeglib, qhull…) | |
| **inline `{_wrap.cpp,_wrap.cxx,_wrap.c,-upb.c,-upb.h}`** | **167-169** | **SWIG/upb generated suffixes** | **anonymous loop literal buried in `isGeneratedPath()` — logically belongs next to `kGeneratedMarkers`; the user pointed at exactly this** |
| `kGeneratedMarkers` (16) | 180-184 | generated-file path markers (.pb., moc_, .tab.…) | |
| `kVendoredStems` (7) | 204-206 | single-file-lib stems (qcustomplot, sqlite3…) | |
| `kVendoredExact` (12) | 207-210 | single-file-lib full basenames (json.hpp…) | |
| `kLicenseMarkers` (6) | 238-244 | permissive-license banner phrases | authority-curated heuristic |
| `kBannerMarkers` (2) | 273-276 | generated-banner phrases (coco/r…) | authority-curated heuristic |
| `kTestDirNames` (7) | 347-349 | test directory names | |
| `kTestStemSuffixes` (6) | 376-377 | test-file stem suffixes (_test, -spec…) | |
| minified thresholds | 311-313 | avg-line-length / sample window | numeric, like Thresholds |

Even *within* the canonical file there is un-named drift: the 167-169 suffix list does the same job as `kGeneratedMarkers` but is an inline `for (... : {...})` literal instead of a named array. Step 0 of any cleanup: give it a name (`kGeneratedSuffixes`) right there.

### Tier B — drift OUTSIDE the canonical home (the "расползлось" the user is reacting to)

| File:line | Constant | Classifies | Conflict with canonical |
|---|---|---|---|
| `include/archcheck/rules/area_of.h:18-20` | `wrapperDirs()` (8) | wrapper dirs to strip for module name: `src source sources include includes inc lib libs` | new concept, no canonical equivalent |
| `include/archcheck/rules/area_of.h:23-47` | `noiseDirs()` (25) | "noise" dirs (not a module boundary) | **overlaps `kExcludedDirNames` + `kVendoredDirNames` + `kTestDirNames` but diverges**: spells `cmake-build-debug/release` explicitly (canonical uses `kCmakeBuildPrefix`), adds `testing examples example mock mocks generated _build _deps node_modules` that no canonical list has |
| `include/archcheck/rules/area_of.h:62-64` | inline `isNoiseSeg` | prefixes `build_ build- mock`, substring `override` | canonical has none of these |
| `src/rules/sf9_no_cycles.cpp:69-71` | `kImplMarkers` (14) | inline/template impl suffixes: `-inl.h _inl.h .tmpl.h .impl.h .inl .ipp .icc .tcc .tpp .hxx _impl.hpp _impl.hh _impl.h _impl.hxx` | partially overlaps `kHeaderExtensions` (.inl/.ipp/.tpp/.hxx) but is a *suffix* notion; no canonical home |
| `src/rules/sf9_no_cycles.cpp:86` | inline `{.hpp,.hh,.h}` | header extensions for stem extraction | **should be `kHeaderExtensions`** |
| `src/rules/lakos_god_headers.cpp:12-13` | `kKnownPch` (4) | precompiled-header basenames: `pch.h stdafx.h precompiled.h precompiled_header.h` | no canonical home |
| `src/scan/include_resolver.cpp:15-16` | `kMirrorPrefixes` (6) | amalgamation/dist/generated dir prefixes: `single_include/ amalgamate/ amalgamated/ dist/ generated/ release/include/` | overlaps the "generated"/mirror notion in `kGeneratedMarkers` (`/generated/`); resolver-internal — **Phase 2 only**, behind a YAML key if at all |

### Tier C — borderline (file/test classification, different axis — decide, likely secondary)

| File:line | Constant | Note |
|---|---|---|
| `src/scan/local_complexity_drift.cpp:39-40` | `kTestSymbols` (6): `TEST TEST_F TEST_P TYPED_TEST BENCHMARK MOCK_METHOD` | "what is test code" axis, but **macro-level**, not path-level. Could co-home with test classification; lower priority. |

### Tier D — explicitly OUT of scope (NOT file classification — do NOT move)

| File:line | Constant | Why it stays |
|---|---|---|
| `src/scan/flag_argument_scan.cpp:23-24` | `kPrefixes` (10): `enable disable use force skip with without allow is no` | ARG.1 rule's **semantic vocabulary**, not file classification. Candidate for a *separate* rule-vocabulary config surface someday — flag it, don't fold it here. |
| `src/scan/duplication/token_normalizer.cpp:15-29` | C++ keywords (53+) | C++ language definition — **FIXED by the standard**, never config |
| `src/scan/duplication/token_normalizer.cpp:39-41` | operators (28) | same — language tokens |
| `src/scan/function_body_scan.cpp:149-150` | `kQualifiers` (7): `const noexcept override final & && mutable` | C++ language — FIXED |
| `src/scan/include_resolver.cpp:47-51` | `kStdCHeaders` (~25): `assert.h ctype.h errno.h …` | **the C standard library header set — FIXED by the C standard**, like the C++ keyword list. A user would never override "what is a C standard header". Reclassified from Tier B after review. Stays in the resolver. |

### Consumers (the call surface a runtime redesign must thread through)

`#include "archcheck/scan/file_classification.h"` →
`src/scan/project_files.cpp` (kProjectExtensions, kHeaderExtensions, isExcludedDirName) ·
`src/git/git_object_file_source.cpp` (isExcludedDirName) ·
`src/scan/duplication/duplication_scanner.cpp` (isGeneratedPath) ·
`src/scan/satd_scan.cpp`, `src/scan/test_co_evolution.cpp` (isVendoredFile, pathHasVendoredDir, pathHasTestDir, isTestBasename) ·
`src/scan/defect_attractor.cpp`, `src/scan/god_file_growth.cpp`, `src/scan/local_complexity_drift.cpp`, `src/graph/graph_builder.cpp` (via SourceSnapshot/exclusions) ·
`include/archcheck/scan/authored_scope.h` (the widest user: pathHasVendoredDir, pathHasTestDir, isTestBasename, isVendoredBasename, isGeneratedPath, hasGeneratedHeader, hasMinifiedContent, hasVendorLicenseHeader).

These are called per-file in hot loops (graph build, dedup pass). Any runtime-config delivery must not regress that.

## Two phases

### Phase 1 — consolidate (behavior-preserving, no YAML yet)

Pull Tier B into the canonical home so there is **one definition per concept**, callers use shared predicates:
- Name the inline 167-169 suffix list (`kGeneratedSuffixes`) next to `kGeneratedMarkers`.
- `sf9_no_cycles.cpp:86` → use `kHeaderExtensions`. Move `kImplMarkers` into the header (new `isInlineImplFile()` / `kImplSuffixes`).
- `lakos_god_headers.cpp` `kKnownPch` → header (`isKnownPchBasename()`).
- `include_resolver.cpp` `kStdCHeaders` → leave in place (Tier D, FIXED — C standard). `kMirrorPrefixes` → Phase 2 only (resolver-internal; consolidate only if it earns a YAML key).
- **`area_of.h` is the delicate one.** Its `noiseDirs()` is *semantically* "exclude from module attribution", which is close to but not identical with "exclude from scan". Do NOT blindly redirect it to `isExcludedDirName || isVendoredDirName || isTestDirName` — that changes which tokens match (canonical lacks `examples/testing/mock/generated/_deps`). Either (a) extend the canonical lists to cover them and prove no scan-side regression with fixtures, or (b) keep a small `areaOf`-specific extra set but build it *on top of* the shared predicates. Decide with eyes on the corpus, pin with tests.

DoD phase 1: `grep` finds each Tier-B concept defined **once**; dogfood still 0 violations; all tests green; corpus spot-check (a few repos) shows identical vendored/test/generated/area classification before/after.

### Phase 2 — parameterize from YAML (the new capability)

Introduce **one runtime classifier** — defaults = today's embedded constexpr values, extendable from `.archcheck.yml`. Sketch:

```yaml
# .archcheck.yml  (all keys optional; merge ON TOP of curated defaults)
classification:
  extra_vendored_dirs:   [my_vendor, _imports]
  extra_test_dirs:       [acceptance]
  extra_generated_markers: [".myproto."]
  extra_project_extensions: [".cu"]      # CUDA, e.g.
```

Mirror #041's merge model: **the user writes only additions**; curated authority-backed defaults are never silently replaced (a project adds project-specific tokens, it does not delete `third_party`). Default to `extra_*` additive semantics; consider a `replace: true` escape hatch only if a concrete need appears.

## Progress

### Phase 1 — partial (2026-06-27): safe behavior-preserving consolidation

Done (3 concepts given a named home in `file_classification.h`, zero behavior change, all consumers redirected):
- `kGeneratedSuffixes` — the anonymous inline `{_wrap.cpp…-upb.h}` loop literal inside `isGeneratedPath()` (the user's pointed example) is now a named `static constexpr` array next to `kGeneratedMarkers`.
- `kImplSuffixes` + `isInlineImplFile()` — lifted from `src/rules/sf9_no_cycles.cpp`'s private `kImplMarkers`. `sf9` now `#include`s the header, `componentStem` iterates `scan::kImplSuffixes`, `isImplName` removed in favour of `scan::isInlineImplFile`. Case-sensitive match preserved.
- `kPchBasenames` + `isKnownPchBasename()` — lifted from `src/rules/lakos_god_headers.cpp`'s private `kKnownPch`. `isPchName` delegates; exact case-sensitive basename match preserved.

Verified: `ninja` clean, **581 test cases / 2056 assertions green**, dogfood `archcheck src/ include/ tests/` → 0 violations, clang-format 18.1.3 clean, cppcheck clean, lizard 0 new warnings, coverage PASS (lines 92.2% / functions 96.4% / branches 58.1%). Tests exercising the moved paths confirmed live (`file_classification_test.cpp:199` _wrap, `lakos_god_headers_test.cpp` pch, `sf9_no_cycles_test.cpp` inline-split).

Committed: the header symbols (`kGeneratedSuffixes`, `kImplSuffixes`/`isInlineImplFile`, `kPchBasenames`/`isKnownPchBasename`) landed in **ac76ab6** (swept in with #151, shipped in v0.1.1); the consumer wiring — `sf9_no_cycles.cpp`/`lakos_god_headers.cpp` redirected to the shared predicates, local `kImplMarkers`/`kKnownPch` removed — in **5916b77**. NB (verified + decided): v0.1.1 (tag `fa57c48`, **published** GitHub release) ships these 4 header symbols as dead code — `ac76ab6` (the #151 sweep-in) is an ancestor of the tag, the consumer wiring `5916b77` came two commits after it. Inert: unused `inline` ⇒ no codegen, no build warning, v0.1.1's own self-scan was still 0. **Decision: accept, no action.** A published tag is not rewritten for a cosmetic source-level smell (breaks checksums / anyone who fetched it); master is already clean (`5916b77`), so the next patch inherits the fix for free. Option C (fold into a future v0.1.2) available but not worth a dedicated release.

### Phase 1b — area_of merge (2026-06-27): the real duplicate, now deduped + measured

`area_of.h`'s `noiseDirs()` duplicated (and had drifted from) the vendored/test/excluded
dir lists in `file_classification.h` — the concrete "расползлось" the user pointed at. Now
`isNoiseSeg` delegates to the canonical `scan::isExcludedDirName / isVendoredDirName /
isTestDirName`; only the genuinely area_of-specific scaffolding tokens that are NOT
scan-exclusions (`examples/ example/ mock(s)/ generated/ testing/ _build`) stay local as
`isAreaExtraNoise`. `wrapperDirs()` (module-name stripping) stays — a distinct concern.

**#115 impact — measured, not assumed** (the user gated on this; `area_of` feeds the
lateral-drift / bidirectional-coupling rules). Replicated old/new `areaOf` in Python,
validated against the C++ pinning test, ran over a 205-repo corpus sample:
- raw: **5.2% of module-files flip** (5526/105953), 12 modules vanish per 205 repos — *looked*
  material, would have wrongly read as "don't merge";
- **decisive skeptic-check**: the graph the rules run on contains **authored files only**
  (`graph_builder.cpp:59` drops `!authored`; `authored_scope.h:39` = `pathHasVendoredDir ||
  pathHasTestDir || …` — the SAME canonical predicates). The dirs that flip are exactly the
  ones already excluded before the rule. Re-measured among authored (in-graph) files:
  **0 flips / 100427 files (0.0000%)**. The 5.2% was entirely on non-graph vendored/test files.
- eyeball of the 12 vanished modules: 11/12 correctly vendored/test (3rdparty, freetype, zlib,
  deps, dependencies, third-party, tst); 1 borderline (ggml-cuda via a `vendors/` subdir + `.cu`
  not in scan extensions). So the merge is a correctness improvement AND a proven `#115` no-op.

Verified: 585/585 tests (incl. 54 drift assertions unchanged), dogfood 0, format/cppcheck/lizard clean.
Pinning test `tests/unit/rules/area_of_test.cpp` updated to the merged behavior. Not committed (awaiting command).

Still deferred:
- **`sf9_no_cycles.cpp:86` inline `{.hpp,.hh,.h}`** → `kHeaderExtensions`: **NOT equivalent** (3 vs 8 extensions — adds `.hxx/.ipp/.tpp/.inl/.inc`). Swapping changes `componentStem` behavior. Needs a fixture proving intent → fixture phase.
- **`kStdCHeaders`** — reclassified Tier D (FIXED), left in resolver.
- **`kMirrorPrefixes`** — Phase 2 only.
- **Phase 2** — the YAML-parameterizable classifier class (the stated goal), the one large remaining piece.

## Open design questions (decide in the task, do not pre-bake)

1. **constexpr → runtime delivery.** The canonical funcs are free `inline bool(path)` over `constexpr` arrays, called in hot loops by ~10 consumers + `authored_scope.h`. Options:
   - (a) **thread a `FileClassifier` object** (holds merged vectors) through every consumer — explicit, testable, no globals, but touches every call site and signature;
   - (b) **init-once process global** built from `Config` at startup, read by the existing free functions — keeps signatures, but global mutable state (determinism + test-isolation cost);
   - (c) **hybrid**: keep `constexpr` defaults as the zero-config fast path, build a runtime overlay only when `.archcheck.yml` adds tokens; free functions consult the overlay if present.
   Recommendation to evaluate first: **(a)** — it matches how `Config` is already threaded into rules (`makeDefaultRuleSet(const Config&)`), and archcheck loads config exactly once. Quantify the call-site churn before committing.
2. **Which lists get a YAML key?** Not all. User-facing: extensions, vendored dirs, test dirs, generated markers/suffixes. **Embedded-only** (authority-curated heuristics where user input is nonsensical): `kLicenseMarkers`, `kBannerMarkers`, `kVendoredStems/Exact`, minified thresholds. The task must pin the exact surface.
3. **Extend vs replace** (see Phase 2) — default extend.
4. **area_of reconciliation** (see Phase 1) — the one real behavior risk.
5. **Where does the class live?** `include/archcheck/scan/file_classification.h` stays the defaults home; the runtime object could be `FileClassifier` in the same header or a new `file_classifier.h`. One file or one class, per the user's ask.

## Fixtures & Tests (mandatory — CLAUDE.md)

- Phase 1: regression fixtures proving area_of / sf9 / god-header / resolver classify **identically** before/after the merge (this is the risky part).
- Phase 2: `fixtures/classification_config/pass/` with an `.archcheck.yml` adding `extra_vendored_dirs` → a project-specific vendor dir gets excluded that the default would scan; `fail_*` for an unknown key under `classification:` → exit 2 (consistent with the loader's `thresholds:` handling); a test that **omitting** the block leaves every default intact (zero-config unchanged).
- Dogfood: archcheck must stay at 0 self-violations (ironic + on-brand: this task removes the very kind of duplication archcheck flags).

## Definition of done

- One definition per file/dir/path classification concept (Tier A+B unified); `grep` proves no second copy.
- A single classifier file/class; defaults embedded; the chosen subset overridable/extendable from `.archcheck.yml` via additive `extra_*` keys, merged on top of defaults.
- Loader validates the `classification:` block; unknown key → exit 2.
- Determinism preserved; hot-loop performance not regressed (spot-check on a large corpus repo).
- Tier D untouched; Tier C decision recorded.
- `docs/config_format.md` updated with the new block; README "Default thresholds" area extended or a sibling "Classification defaults" note added.
- JOURNEY.md: a line on overturning #041 decision-5 and why.

## Do NOT do

- Do NOT fold C++ language tokens (keywords, operators, qualifiers — Tier D) into config. They are FIXED by the standard.
- Do NOT replace curated defaults by default — additive merge only.
- Do NOT silently change area_of's module attribution while merging its noiseDirs — pin behavior with fixtures first.
- Do NOT build a generic "rule engine" config; this is file-classification only. The flag-prefix vocabulary (`flag_argument_scan.cpp`) is a *separate* surface — note it, do not absorb it.
- Do NOT commit without an explicit command.

## Key decisions

| Decision | Rationale |
|---|---|
| New task, not a re-open of #041 | #041 is closed and its thresholds/extension-dedup scope is done; this is a larger, later problem (drift recurred + user asked for YAML) |
| Supersede #041 decision-5 | Its YAGNI basis ("no user") no longer holds as of 2026-06-27 |
| Additive `extra_*` merge | Authority-backed defaults must not be silently deleted; user adds, not replaces |
| Not every list gets a YAML key | License/banner/stem heuristics are curated; user input there is nonsensical |
| area_of merge is the risk | Its noiseDirs semantics differ from scan-exclusion; needs fixtures, not a blind redirect |
