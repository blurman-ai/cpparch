# [SCAN][DUPLICATION][BUILD] Move `fp_corpus_eval` out of the shipped `archcheck_core`

**Creation date:** 2026-06-05
**Start date:** 2026-06-05
**Completion date:** 2026-06-05
**Status:** completed
**Module:** SCAN/DUPLICATION + BUILD
**Priority:** minor
**Difficulty:** S
**Blocks:** —
**Blocked by:** —
**Related:** #082 (alignment umbrella — parent, Slice 5a/5b), #083 (precision — the only real consumer of an honest corpus metric)

## Goal

Remove the research/QA precision-evaluation harness from the main shipped library
`archcheck_core`, so the product code doesn't carry a non-product placeholder.

## Context (where the task comes from)

[src/scan/duplication/fp_corpus_eval.cpp](../../src/scan/duplication/fp_corpus_eval.cpp)
is compiled into `archcheck_core` ([src/CMakeLists.txt](../../src/CMakeLists.txt) line ~13),
but:

- it's used **only by tests** ([tests/duplication_fp_corpus_eval_test.cpp](../../tests/duplication_fp_corpus_eval_test.cpp)),
  not on the runtime path of `--duplication`;
- `evaluateAgainstCorpus` — a **placeholder** ("assume all pairs are TP"), it doesn't compute a real
  metric (only the degenerate case is tested).

In #082 (Slice 5b) this is marked as "internal QA tooling outside the user-facing surface" and
deliberately left in place — relocation is a structural build change, separate from
the alignment contracts.

## What's needed (pick one)

1. **Move** into a test-only target (remove from `archcheck_core`, link only into
   `archcheck_tests`); or a separate `research/` directory outside the main build.
2. **Implement** `evaluateAgainstCorpus` for real, if the corpus metric is needed
   (then it's part of #083).
3. **Explicitly isolate** as a research namespace/comment marked "not product".

Recommendation: option 1 (test-only target) — minimal and honest.

## Acceptance criteria

- [x] `archcheck_core` (the shipped lib) no longer contains placeholder corpus-eval code.
- [x] The test `duplication_fp_corpus_eval_test` continues to build and pass.
- [x] Build green; coverage thresholds hold.

## How it was done (2026-06-05)

Option 1 chosen (test-only target), the files left in place:

- `src/CMakeLists.txt` — `scan/duplication/fp_corpus_eval.cpp` removed from the
  `archcheck_core` source list, a pointer comment in its place.
- `tests/CMakeLists.txt` — `${CMAKE_SOURCE_DIR}/src/scan/duplication/fp_corpus_eval.cpp`
  added to the `archcheck_tests` sources (compiled straight into the test binary).
- `include/archcheck/scan/duplication/fp_corpus_eval.h` — the header marked as a
  research/QA test-only harness (not part of the shipped `archcheck_core`).

**Verification:** `ar t libarchcheck_core.a` — no `fp_corpus_eval.o`; `nm` — no
corpus symbols in the shipped lib. The test `duplication_fp_corpus_eval_test` builds and
passes. Full gate: clang-format/cppcheck/lizard clean, build, 344/344, smoke,
coverage 91.4/95.8/57.2 — PASS.

**Note:** the placeholder `evaluateAgainstCorpus` stayed a placeholder — computing the real
corpus metric wasn't required here; if needed, it's part of #083.
