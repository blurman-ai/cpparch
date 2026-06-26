# [SCAN] #092: scale-independent candidate generation (rare-token index missed clones in big repos)

**Date created:** 2026-06-07
**Date started:** 2026-06-07
**Status:** completed
**Date completed:** 2026-06-07
**Module:** SCAN (duplication)
**Priority:** critical (detector correctness)
**Related:** #091 (large-function fragmentation), #056 (token dedup), [duplication_architecture.md §3.3](../../docs/duplication_architecture.md)

## Goal / bug
Candidate generation was **dependent on project size**: the same copy-paste
was detected in a small repository and **disappeared** in a large one. This is a
correctness defect — copy-paste stays copy-paste at any project size.

## Root cause (measured)
Candidates were built via an inverted index over "rare" tokens:
[`clone_index.cpp`](../../src/scan/duplication/clone_index.cpp) — a token is "rare"
if its document-frequency `≤ rareDfCap (=4)` **across the whole corpus**, a pair needs
`≥ minSharedRare (=2)` shared rare tokens. In a large project the pair's shared tokens
become frequent → stop being "rare" → the pair **doesn't make it into the
candidates** → is never scored at all.

**Proof** — the same `LibreSprite/src/doc/algo.cpp`, the same spline functions,
only the corpus grows:

| corpus | fragments | spline clone found? |
|---|---:|:---:|
| 1 file | 10 | YES |
| 51 | 99 | YES |
| 501 | 1090 | YES |
| **1211 (whole repo)** | 3067 | **NO** |

Exactly the hole that [§3.3](../../docs/duplication_architecture.md) predicted:
*"absolute df fails on large trees… Fallback needed: k-gram fingerprints".*

## Fix
Added **scale-independent** candidate generation — robust winnowing k-gram
fingerprints (MOSS-style) over the fragment's normalized token-seq:
- hash every `fpK=5`-gram (FNV-1a), keep the min of each `fpWindow=8` window
  (re-emit on a change of minimum) → density ~1/8, **depends only on the pair's content**;
- two functions with a shared fingerprint → candidates at any corpus size;
- **additive**: combined with rare-token candidates (old behavior preserved).

**Anti-explosion:** `fpMaxPostings=20` — a fingerprint occurring in > 20 fragments
is dropped as a boilerplate idiom. A distinctive clone run is rare in the absolute at
any N, so this does **not** reintroduce scale-dependence for real clones.
(Without the cap there were 972k candidates on LibreSprite; with cap=20 — 65k.)

## Verification
- [x] the spline triple `algo.cpp` is now reported **by default** (`465-533 ↔ 540-611 ↔ 618-689`).
- [x] hermetic regression test `duplication_fingerprint_test.cpp`: a clone pair whose shared
      tokens are made frequent by fillers — **not a candidate** without fingerprints, **a candidate** with them.
- [x] full suite green (**356 tests**); precision-eval (`duplication_fp_corpus_eval`) didn't regress.
- [x] **recall on the corpus grew, and the new pairs are real clones** (irrlicht sample:
      `CD3D8NormalMapRenderer ↔ CD3D9NormalMapRenderer` EXACT, `CTRTextureFlatWire ↔
      CTRTextureGouraudWire` EXACT, `CTarReader ↔ CWADReader` EXACT — all pass the
      strict gate w≥0.75 ∧ line≥0.50). The pairs were hidden by the scale-dependent index.

Pairs across the corpus, **before → after** (fingerprints):

| repo | before | after |
|---|---:|---:|
| Catch2 | 0 | 0 |
| GWToolboxpp | 12 | 149 |
| Kartend | 42 | 62 |
| IrredenEngine | 4 | 116 |
| LibreSprite | 10 | 78 |
| irrlicht-1.8.3 | 41 | 467 |
| AetherSDR | 72 | 390 |
| BambuStudio | 129 | 1152 |

## Cost
- Time ~×3-4 (BambuStudio 7 s → 20 s — still gate-scale, <30 s on 3173 files).
- More candidates (BambuStudio ~320k) — std::map memory ~tens of MB, acceptable.
- Far more reported pairs — this is **correct** (before ~90% of clones were lost);
  the volume for CI is silenced by baseline mode (a separate concern, not the detector).

## Changed files
- `include/archcheck/scan/duplication/clone_index.h` — IndexOptions: `fingerprints/fpK/fpWindow/fpMaxPostings`.
- `src/scan/duplication/clone_index.cpp` — `kGramHashes` + `fingerprintsOf` + `addFingerprintCandidates`, call in `buildIndex`.
- `tests/duplication_fingerprint_test.cpp` (new), `tests/CMakeLists.txt`.

## Tail
- `fpMaxPostings=20` cuts clone **classes** larger than 20 (usually boilerplate). If
  catching large classes is needed — raise the cap pointwise.
- The `reports/nicad_*` reports were computed before #092 — the archcheck numbers there are now understated
  (recall grew); recompute on the next update of the comparison.
