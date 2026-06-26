# [BUG][DUPLICATION] P0.2 whole-file suppress silences the TP "extracted code into a module, kept the original"

**Created:** 2026-06-26
**Status:** new
**Module:** SCAN / DUPLICATION
**Priority:** major
**Related:** #052/#056 (clone detector), #127 (vendored/generated exclusion), #147 (dup OOM),
P0.4-removal precedent (docs/duplication_architecture.md "History of P0.4")

## Symptom (dogfood on a real refactor, repo leadline)

In a third-party project (`leadline`) the user extracted the classifier into a new module
`src/geomorph/feature_support.cpp` (functions `classifyAll`, `Support`, `makeSupport`,
`featureSupported`, `bracketGap`, `medianSoundingSpacingGrid`, `soundingSupported`,
`quantile`, `faceDepthMean`, `triMinAngleDeg`, …), **but left near-identical copies
in `tools/inspect_at.cpp`** (the pointwise dedup was deferred). This is exactly the "missing reuse edge" —
the detector's main target TP class (§1 `duplication_architecture.md`).

`archcheck --duplication .` over the leadline tree: **59 pairs above threshold, NONE involving
`feature_support.cpp`.** Only the small boilerplate `makeDirs`
(`gallery_export.cpp ↔ inspect_at.cpp`, STRUCTURAL 0.996) surfaced.

Isolating down to two files (only `feature_support.cpp` + `inspect_at.cpp`) confirms it:

```
$ archcheck --duplication <2 files>
scanned 3 files, 82 fragments, 1521 candidate pairs
reported 0 pairs above threshold (1 whole-file clone group(s) suppressed)
```

**0 pairs**, even though the function bodies match almost character-for-character (for `bracketGap` the only
difference is a comment). It is precisely the P0.2 whole-file guard that suppresses it (counter `1 ... suppressed`).

## Root cause (exact)

`wholeFileClonePairs` (`src/scan/duplication/duplication_scanner.cpp:281`), called from
`phase9WholeFileSuppress` (same file, `:316`), gated by `opts.enableWholeFileGuard`:

```cpp
const int minFrags = std::min(fragsPerFile[fileA], fragsPerFile[fileB]); // fragments of the SMALLER file
if (minFrags >= 2 && n * 5 >= minFrags * 4)   // n matches >= 80% of fragments of the SMALLER file
  result.insert(k);                            // -> the whole file pair is excluded from actionable
```

The condition is one-sided: "≥80% of the fragments of the **smaller** file matched". For our case:

- `feature_support.cpp` (smaller) ≈ 10 function-fragments, ~8+ match `inspect_at.cpp`
  → `n ≥ 0.8·minFrags` → triggers → **ALL pairs** between the files are silenced.
- `inspect_at.cpp` (larger) ≈ 80 fragments; these ~8 matches cover ~10% of its volume.

That is, the detector treats "smaller file ⊂ larger" as move/copy/vendored-twin. But this is
NOT a file move: both files are alive, the original did not disappear (rename), and the overlap is a small share
of the larger file. P0.2 **conflates two different classes**:

| class | A vs B | overlap | what it is | P0.2 now |
|---|---|---|---|---|
| move / vendored twin (target FP) | ~equal in size | ~all of A AND ~all of B | FP, silence correctly | silences ✅ |
| **extract-into-module, original alive** (our TP) | B small, A large | all of B, but a small share of A | **TP, report** | silences ❌ |

The discriminator that is missing: a true twin covers **both** sides
(`n ≥ 0.8·maxFrags` too), whereas an extraction covers only the smaller one. Currently only the
smaller side is checked → false suppression of a TP.

## Why this matters

1. It suppresses the **most valuable** dup class — "extracted shared code but forgot to delete the
   original" (extract-leave-original) — which is exactly the "missing reuse edge" the §1
   detector exists for. The CLEANER the extraction, the more reliably P0.2 hides it (ironically).
2. Silently: only the line `N whole-file clone group(s) suppressed`, without listing what exactly.
3. Double miss for CI: `--duplication` is advisory and not part of the default scan/diff gate,
   so there will not even be a reminder. A refactor that "left a duplicate" passes unnoticed.
4. Matches a principle already established by the project (removal of P0.4/P0.7/P0.8,
   `duplication_architecture.md`): *"a guard that silences a real TP for the sake of an FP class
   poorly separated by the current fragmenter is pure recall tax"*. P0.2 in its current form is
   exactly that: to catch a file-move, it strangles extract-leave-original.

## Repro (fixture attached + verified)

Synthetic fixture (our own code, no external sources):
`fixtures/duplication/wholefile_extract_fp/`
- `extracted_geom.cpp` — module of 3 helpers (`triMinAngleDeg`/`quantileSorted`/`bracketGap`);
- `original_geom.cpp` — large original: the same 3 helpers verbatim + 4 unrelated functions.
B ⊂ A (extraction, original alive).

**Bug (current):**
```
$ archcheck --duplication fixtures/duplication/wholefile_extract_fp
scanned 2 files, 10 fragments, 14 candidate pairs
reported 0 pairs above threshold (1 whole-file clone group(s) suppressed)
```

**Contrast (proves it is the subset form being silenced, not the threshold).** If you add 4 OWN
full-size functions to the smaller file (both files ~7 fragments each, 3 shared, none ⊂):
```
scanned 2 files, 14 fragments, 36 candidate pairs
reported 3 pairs above threshold (0 whole-file clone group(s) suppressed)
  a.cpp:13-20  <->  b.cpp:11-18  (EXACT, weighted=1, line=1)
  a.cpp:25-28  <->  b.cpp:23-26  (EXACT, weighted=1, line=1)
  a.cpp:33-38  <->  b.cpp:31-36  (EXACT, weighted=1, line=1)
```
→ the same 3 clones are detected EXACT/weighted=1; the only difference is the form "B ⊂ A" vs
"A≈B". Confirms the one-sided `minFrags` as the cause.

The real source of the case is `leadline:src/geomorph/feature_support.cpp` ↔
`leadline:tools/inspect_at.cpp` (same result: 0 pairs, 1 suppressed).

## Proposed direction (not a prescription for implementation)

Any of the options (or a combo), plus a regression fixture:

- **Two-sided coverage:** count a whole-file clone only when the overlap covers
  a high share of **both** files: `n ≥ 0.8·minFrags && n ≥ 0.8·maxFrags`. Then a move/twin
  (A≈B, both covered) is silenced, while an extraction (B⊂A, A weakly covered) is reported.
- **Cross-check with git-rename:** P0.2 is motivated in the docs by "git rename/move/vendored". Gate it
  by actual rename-similarity from git (rename/move), rather than an overlap heuristic over
  fragments — in a move the original DISAPPEARS from the old place, in an extraction the original stays put.
- At minimum — make the suppression **visible/optional** (`--show-suppressed` or a list of
  suppressed pairs in the report), so the silent miss stops being silent.

## Acceptance (DoD)

- Regression fixture `duplication_fp_guards_test.cpp`: "extract-into-module with a live original
  (B small, B⊂A, A large) → the pair is REPORTED, not whole-file-suppressed". The ready input and both
  measurements are in `fixtures/duplication/wholefile_extract_fp/` (subset → 0 pairs; contrast variant
  "A≈B" → 3 pairs). Inline test style — as in the existing P0.x cases.
- Existing move/vendored-twin FP fixtures (A≈B, both covered) are still silenced.
- archcheck self-scan and corpus — no new FP (as with removal of P0.4: check the delta of pairs).
- If the git-rename gate is chosen — document it in `duplication_architecture.md` (P0.2).

## Code locations (for the implementer)

- `src/scan/duplication/duplication_scanner.cpp:281` — `wholeFileClonePairs` (rule
  `n*5 >= minFrags*4`, one-sided `minFrags`).
- `src/scan/duplication/duplication_scanner.cpp:316` — `phase9WholeFileSuppress`
  (opt. `opts.enableWholeFileGuard`).
- `src/cli/preview_commands.cpp:139` — report line `... whole-file clone group(s) suppressed`.
- `docs/duplication_architecture.md:241` — description of P0.2 (update when fixing).
