# [RULES][DRIFT] DRIFT.3 — bidirectional area coupling (mutual connectivity)

**Created:** 2026-06-06
**Started:** 2026-06-06
**Completed:** 2026-06-06
**Status:** completed
**Module:** RULES/DRIFT + GRAPH
**Priority:** major
**Complexity:** L
**Blocks:** —
**Blocked by:** —
**Related:** #009 (DRIFT rules), #082 (alignment umbrella), [docs/research/drift_signal_validation.md](../../docs/research/drift_signal_validation.md) (corpus proof)

## Goal

Add a narrow rule DRIFT.3 that catches the **appearance of a mutual dependency between
modules/areas at the AGGREGATE level** — when module `A` starts depending on
module `B`, while `B` already depends on `A`, **but through different files**, so that
there is NO cyclic include path.

### How this is NOT DRIFT.2 (important — otherwise the rule is redundant)

At the file/component level `A→B` + `B→A` = a 2-node cycle, and it is **already caught by SF.9 /
DRIFT.2**. DRIFT.3 is strictly about the **area/module level**: different files in each module
(`hal/gpio.h → ui/x.h` and `ui/btn.cpp → hal/gpio.h`) produce a mutual dependency of modules
`hal ↔ ui` **without** a cyclic include. This is Lakos "non-levelizable design": the modules
can't be built/tested separately, even though no component is in a cycle.

**DRIFT.3 must NOT overlap DRIFT.2:** if the mutuality forms a real
file-cycle — that's DRIFT.2's domain, DRIFT.3 stays silent (no double report).

## Rationale (by data)

Corpus validation ([drift_signal_validation.md](../../docs/research/drift_signal_validation.md)):
of 1903 cross-area events, **294 (15%) are bidirectional `A↔B`**, and this is a **real
actionable signal** (unlike raw cross-area, where ~50% is normal). Examples from
the corpus: `src/hal ↔ src/ui` (HAL must not mutually depend on UI), `Source/Game ↔
Source/Renderer`, `core ↔ inspect`, `editor ↔ engine`. The narrow rules DRIFT.1/DRIFT.2
currently do NOT cover this class.

**Proof of non-redundancy with DRIFT.2 (corpus):** of **65 commits** with
a bidirectional area-pair only **9** had a real file-cycle (`grown_cycles>0`,
already caught by DRIFT.2). The remaining **56 (86%)** are `grown_cycles==0`: mutual coupling
of modules through different files, without a cyclic include → **DRIFT.2 misses them**.
So DRIFT.3 catches 56 commits that DRIFT.2 doesn't see. (Reproducible:
script in `experiments/ai_repo_run/*_graph_drift.jsonl`, filter bidirectional ∧ grown_cycles==0.)

## What's needed

1. Define an "area" for the graph. **BEFORE the rule** — fix area-detection:
   - don't count `src↔include` as cross-area (headers of the same area);
   - exclude build/output/vendor/test/generated directories;
   - don't spawn phantom deps from directory renames.
   (Without this DRIFT.3 will drown in noise — see the validation.)
2. Implement DRIFT.3: a new edge `A → B`, when the baseline already has a path `B → A`
   at the area level → bidirectional coupling. One class = one file (OCP).
3. Decide the mode: advisory (report) vs gate. Recommendation — start **advisory**,
   raise to gate after a corpus run and a precision measurement (like DRIFT.2).
4. Fixtures: `fixtures/drift_bidirectional/pass|fail_*`.

## Acceptance criteria

- [x] Area-detection doesn't produce `src↔include`/build/vendor/rename false cross-area.
- [x] DRIFT.3 catches the appearance of `A↔B` and does NOT fire on unidirectional `→base`.
- [x] **DRIFT.3 does NOT duplicate DRIFT.2:** if the mutuality = file-cycle, it reports only DRIFT.2.
- [x] Fixtures: there's a case "area-coupling without file-cycle" (DRIFT.3 fires, DRIFT.2 silent) and "file-cycle" (vice versa).
- [x] Fixtures pass/fail; the rule is a separate class/file.
- [x] Corpus run: DRIFT.3 validated on a real commit; decision — **advisory** (like DRIFT.1/.2 currently), gate question separate.

## Notes

- This is **not** a "cross-area gate" (that's ~50% FP, we don't do it). DRIFT.3 is narrow — bidirectional only.
- Reuse the graph/levelization from the existing `graph/` layer, don't build a new engine.

## How it was done + validation (2026-06-06)

**Implementation** (one class = one file, OCP):
- `include/archcheck/rules/drift_bidirectional_coupling.h` + `src/rules/drift_bidirectional_coupling.cpp`
  — class `DriftBidirectionalCoupling`, id `DRIFT.3`. Wired into `makeDriftRuleSet`
  (after DRIFT.1, before DRIFT.2) and into `src/CMakeLists.txt`.
- **area function:** the first path segment after stripping wrapper directories (`src/include/lib/..`),
  ignoring build/vendor/test/generated → `src↔include` and noise are not counted as cross-area.
- **semantics:** DRIFT.3 fires when, after the diff, modules A and B depend on each other
  (`A→B` and `B→A` at the area level), and were NOT mutual in the baseline. Excludes a direct
  two-file cycle (`hasEdge(to,from)`) — that's DRIFT.2's domain.

**Important correction during the run (born-coupled vs incremental):** the first version required
that the reverse edge `B→A` already be in the baseline (only incremental erosion). The run
on pulp showed that the commit "Add plugin inspector" added **both** sides at once (the module
was born coupled) — and the rule stayed silent. Rewrote it to "mutuality exists NOW and didn't exist
in the baseline" → catches both incremental and born-coupled.

**Fixtures** (`fixtures/drift_bidirectional/`): `fail_new_coupling` (core↔ui → DRIFT.3),
`pass_one_directional` (unidirectional → silent), `pass_file_cycle` (two-file cycle →
DRIFT.3 silent, DRIFT.2 fires). Tests in `drift_fixtures_test.cpp`.

**Live run:** `danielraffel/pulp` @ `705f86e` ("Add plugin inspector") — archcheck
emitted `[DRIFT.3] bidirectional module coupling: 'core' <-> 'inspect'` (matched what
the python probe flagged as bidirectional), alongside DRIFT.1 ×2 + GodHeader. A run of 60
old pulp commits: 0 false DRIFT.3 (no spam).

**Gate:** clang-format/cppcheck/lizard clean, 347 tests (+3 DRIFT.3), coverage 95.6/57.1 — PASS.
