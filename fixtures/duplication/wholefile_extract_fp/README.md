# Fixture: whole-file suppress kills extract-leave-original TP (#151)

Two synthetic files reproducing the P0.2 false-suppression:

- `extracted_geom.cpp` — small module with 3 helpers (`triMinAngleDeg`, `quantileSorted`,
  `bracketGap`), freshly extracted.
- `original_geom.cpp` — the large original: same 3 helpers verbatim PLUS 4 unrelated functions.

The small file is a SUBSET of the large one (extract-to-module, original left in place) — a
true "missing reuse edge", NOT a file move/copy. Authored for this repo (no external code).

## Repro

    archcheck --duplication fixtures/duplication/wholefile_extract_fp

Buggy (current): `reported 0 pairs ... 1 whole-file clone group(s) suppressed`.
Expected (after fix): the 3 helper pairs reported (extracted_geom ↔ original_geom).
