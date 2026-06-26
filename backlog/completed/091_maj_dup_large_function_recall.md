# [SCAN] #091: recall — large functions and headers that NiCad catches but archcheck was missing

**Created:** 2026-06-07
**Started:** 2026-06-07
**Status:** completed
**Completed:** 2026-06-07
**Module:** SCAN (duplication)
**Priority:** major
**Related:** #062 (fragment boundary align), #056 (token dedup), NiCad eval ([reports/nicad_vs_archcheck.md](../../reports/nicad_vs_archcheck.md))

## Goal
Take specific clone pairs that **NiCad found but archcheck missed**,
diagnose the root cause, fix safely (without precision regression), and close
with a regression test.

## Test set (NiCad-found / archcheck-missed)
From the NiCad eval, the "only-NiCad authored" bucket:

- **CASE A — LibreSprite `src/doc/algo.cpp`** (within-file twins):
  `algo_line` (20-101) ↔ `algo_line_float` (105-190), NiCad sim 93;
  `algo_spline_get_y` (536-612) ↔ `algo_spline_get_tan` (614-690).
- **CASE B — AetherSDR headers** (cross-file, declarations):
  `src/core/TgxlConnection.h` ↔ `src/core/PgxlConnection.h` (confirmed
  copy-paste of a connection class; a comment literally says "Same protocol as TgxlConnection").

## Root causes (measured, not eyeballed)
Reproduced via direct calls to `scanForDuplication` / `extractFragments` on
real files (harnesses — there is no `reports/raw/nicad/`; one-off ones in `nicad_work/`).

**CASE A — the `maxTokens=400` threshold cuts twins asymmetrically.**
`algo_line` = 392 tokens → one whole fragment `22-100`. Its twin
`algo_line_float` > 400 tokens → the fragmenter splits it into sub-blocks
(`132-142, 147-166, 170-177`). A whole fragment vs three sub-blocks **does not
align** → the clone is invisible. For a *flat* body > cap (no nested `{}`
to descend into) the fragmenter yields **0 fragments** at all — the function disappears
from the search. The spline trio is the same thing (functions > 400 tokens, split up).

**CASE B — the fragmenter does not emit fragments from declaration files.**
`Tgxl/PgxlConnection.h` → `frags=0`. By design the fragmenter takes only
function/control-block bodies (`{` after `)`); a class declaration
(`class X : public QObject { signals: ... };`) contains no bodies → nothing to compare.

## Fix
**CASE A — `maxTokens` 400 → 600** in `FragmentOptions`
([include/archcheck/scan/duplication/fragmenter.h](../../include/archcheck/scan/duplication/fragmenter.h)).
600 keeps functions up to ~120 lines whole. That is precisely the cure for the asymmetry:
both twins stay whole and match.

**CASE B — NOT fixed (deliberately), and there is nothing to fix.** It turned out: the actual
copy-paste of `Tgxl/Pgxl` archcheck **already catches — in the implementation**
(`PgxlConnection.cpp:111-130 ↔ TgxlConnection.cpp:121-150`, w=0.78 in baseline;
after cap=600 — `74-131 ↔ 79-151`, w=0.88). NiCad additionally showed **the same**
duplicate a second time — in the *declaration* (`.h`). That is, the actionable finding was not
lost; the "miss" was only in the redundant header representation.
Fragmenting declaration bodies would bring back exactly the "expected-pattern" noise that
in the eval is an FP: 9 of 11 "only-NiCad authored" cases in AetherSDR are Qt widget headers,
similar by design. Catching a duplicate declaration at the cost of 9 FPs goes against the
archcheck philosophy (precision under the gate). Declaration copy-paste is out of scope; not a bug.

## Verification
- [x] full suite green: **365 tests** (was 359 before the task).
- [x] regression test `duplication_large_function_test.cpp`: function >400 tokens →
      0 fragments at cap=400, 1 whole fragment at default=600.
- [x] **CASE A actually fixed** on the corpus: `archcheck --duplication LibreSprite`
      now reports `src/doc/algo.cpp:22-100 <-> :107-189` (w=0.75, line=0.87) —
      exactly the NiCad sim-93 clone that was previously lost.
- [x] **precision did not drop** — pairs across the corpus are stable (after vendored-fix #0xx):
      GW 13→12, Kartend 34→42, Irreden 3→4, LibreSprite 10→10, irrlicht 44→41,
      AetherSDR 71→72, Bambu 131→129, Catch2 0→0 — small two-sided deltas,
      no FP explosion.

## How it works
The only lever is `FragmentOptions::maxTokens`. The fragmenter descends into
nested `{}` only when a block is wider than the cap; at cap=600 functions up to ~120 lines
stay as whole comparison units, so a pair of near-identical twins of
the same size aligns into a single clone instead of "whole vs a set of sub-blocks".
The accompanying vendored-fix extends path classification: the curated
`kVendoredLibDirs` catches multi-file libs that sit under their own directory without
a `third_party/` wrapper — this keeps corpus precision stable while
the fragment window is widened.

## Key decisions
- **cap 400 → 600, not "fragment = the whole function"**: a pragmatic step, cures
  the measured asymmetry here and now; the full solution is for #062.
- **CASE B (declarations) not fixed**: the actionable copy-paste is already caught in
  the implementation; fragmenting declaration bodies would bring back 9 FPs (Qt headers) —
  against archcheck's precision gate. A declaration duplicate is out of scope, not a bug.
- **vendored-fix split into a separate commit**: a different cause (path
  classification), even though done in one session for the sake of stable corpus deltas.

## Changed files
- `include/archcheck/scan/duplication/fragmenter.h` — `maxTokens` 400 → 600 (+comment) (commit 56621e3).
- `tests/duplication_large_function_test.cpp` — new #091 regression test (commit 56621e3).
- `tests/CMakeLists.txt` — test registration (commit 56621e3).
- `include/archcheck/scan/file_classification.h` — `kVendoredLibDirs` (commit 0c84294).
- `tests/unit/scan/file_classification_test.cpp` — test for in-tree bundled libs (commit 0c84294).

## Open questions / next
- The spline trio (sim 75) is on the edge of the joint-floor (w≈0.74), not reported. This is
  correct threshold behavior, not a bug; we did not touch the floor.
- #062 (boundary align) overlaps: the final decision about "fragment =
  whole function" will remove the asymmetry completely; cap=600 is a pragmatic step toward it.
- eval artifacts (`reports/`, `docs/research/clone_tools_landscape.md`) left
  untracked deliberately — to be closed in a separate conversation, not as part of #091.

## Summary
**Status:** completed
**Completed:** 2026-06-07
Commits: `56621e3` (cap-fix + regression), `0c84294` (vendored-classification).
Full suite — 365 tests green; gates: clang-format / cppcheck / lizard clean,
coverage PASS (lines 91.1%, funcs 94.8%, branches 57.2%).
