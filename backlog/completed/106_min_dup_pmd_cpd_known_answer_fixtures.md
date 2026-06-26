# [DUPLICATION][TEST] PMD CPD testdata as known-answer fixtures for the token layer

**Created:** 2026-06-11
**Start date:** 2026-06-11
**Status:** wip — approach revised (see Progress)
**Module:** DUPLICATION / SCAN / TEST
**Priority:** minor
**Complexity:** S
**Blocks:** trust in selective normalization (#056/#059)
**Blocked by:** —
**Related:** #053 (line), #056 (token), #059 (precision), #107 (external oracle cross-validation)

## Goal

Take ready-made small C/C++ files from the PMD CPD test resources (BSD license) and
set them up as **known-answer fixtures** for our token layer and selective
normalization. Each PMD file is tailored to one lexer/normalization scenario and
comes paired with a `.txt` expectation — that is exactly how we check the token pass anyway,
only written and reviewed by someone else.

## Context

Source (BSD-3, can be copied with attribution):
`pmd-cpp/src/test/resources/net/sourceforge/pmd/lang/cpp/cpd/testdata/`
https://github.com/pmd/pmd/tree/main/pmd-cpp/src/test/resources/net/sourceforge/pmd/lang/cpp/cpd/testdata

Direct hits into our modes:
- `ignoreIdents.cpp`, `ignoreLiterals.cpp` — ignoring identifiers/literals
  (our key selective-normalization mode, see docs/duplication_architecture.md).
- `literals.cpp`, `listOfNumbers.cpp` (+ variants `_ignored`, `_ignored_identifiers`)
  — literal normalization.
- `continuation*.cpp`, `multilineMacros.cpp`, `preprocessorDirectives.cpp` —
  line continuations and the preprocessor (touches our fast backend).
- `specialComments.cpp`, `tabWidth.cpp`, `unicodeStrings.cpp`, `utf8-bom_*.cpp` —
  lexer edge cases.

The PMD expectations (`.txt`) are their token format, NOT ours. They cannot be compared byte-for-byte;
the expectation must be re-marked for our token output (or for clone pairs, where appropriate).

## What to do

1. Copy the relevant `.cpp` into `fixtures/duplication/normalization/` with an
   `ATTRIBUTION.md` file (PMD, BSD-3, link to the source commit).
2. For each, define the expected behavior of OUR detector (not PMD's):
   with identifier/literal normalization, two "structurally identical but differently named"
   sections must collapse into a clone pair; without — no.
3. Set up a Catch2 test: run the token layer on the fixture → check the expected
   clone pairs / their absence.
4. Record discrepancies (if our lexer behaves differently on continuation /
   utf8-bom / multiline-macros) — that is either a bug or a deliberate difference in the task.

## Definition of Done

- Fixtures live under `fixtures/duplication/`, with attribution.
- A green Catch2 test covering at least ignoreIdents / ignoreLiterals / literals.
- Discrepancies with our lexer are either fixed or explicitly documented.
- Don't touch the detector itself without reason — the task is about coverage, not refactoring.

## Progress (2026-06-11) — approach revised

Full analysis: `experiments/clone_oracle_validation/FINDINGS.md`.

**Key conclusion: end-to-end (CLI `--duplication`) — NOT suitable for PMD snippets.**
PMD testdata are single snippets, while our detector works at the granularity of
functions inside a corpus and keys candidacy on shared **rare tokens** (df≤4)
that survived normalization. We ran 6 synthetic fixtures (renamed/exact/literals) —
none fired, not even a byte-identical pair. The roots (from reading the code):
- `idf=log(N/df)` degenerates to 0 on a corpus of 2 fragments;
- candidacy requires a shared rare token (`buildRareTokenIndex`/`findCandidatePairs`);
- the normalizer (`pushIdentifierToken`, keepCalls) collapses all non-call identifiers
  into a generic `id`, leaving only call names and literals as rare.
=> a generic dummy (`alpha/beta`) is correctly ignored (anti-FP), but our
known-answer fixture is also not caught.

**Revised plan (instead of CLI end-to-end):**
1. Test **the components directly** in Catch2: the output of the `lex()` normalizer on
   PMD `ignoreIdents/ignoreLiterals/literals` (our token stream vs the expectation,
   adapted from PMD `.txt`); `weightedJaccard` on hand-assembled bags.
2. Copy the relevant PMD `.cpp` into `fixtures/duplication/normalization/`
   with `ATTRIBUTION.md` (PMD, BSD-3).
3. For end-to-end cases (if needed) — build a realistic mini-corpus with
   distinctive call names, not 2-function toys.

**Blocking dependency:** before building the suite, resolve the anomaly from #107/§4
(an exact copy with a rare literal is silently dropped) — otherwise "0 pairs" ≠ "no clones".
The downloaded PMD files and repro fixtures: `experiments/clone_oracle_validation/pmd/`.

## Update (2026-06-11, evening) — blocker lifted, approach confirmed

- The blocking anomaly (#107/§4) is **resolved**: it is the P0.6 joint-floor tradeoff (line≥0.50
  on raw lines cuts "heavy" renames), not a bug. So "0 pairs" is now explainable, the suite can be built.
- The first component test by the revised approach already exists and is green:
  `tests/duplication_renamed_recall_test.cpp` (rename-blind tokens vs raw-line floor).
  This is the template for the other known-answer cases based on PMD snippets.
- Next: adapt PMD `ignoreIdents/ignoreLiterals/literals` to component-level
  checks of `lex()` (our token stream), copy into `fixtures/duplication/normalization/`
  with ATTRIBUTION (BSD-3).

## How it works

The fixtures are pairs of files differing along exactly one axis (literal values /
local names / callee name); the component test runs them through `lex()` and compares
the normalized seq: the blind axes must match, the selective axis (callee) — must differ.

## Key decisions

- **Component-level instead of CLI end-to-end**: the detector's candidacy (rare tokens,
  idf) eats tiny snippets — an honest known-answer check lives at the level of
  `lex()`, not the binary.
- **PMD `.txt` expectations not ported**: that is THEIR tokenizer's format; re-marking
  for OUR semantics (seq equality/inequality) — is the essence of the task.
- **An exact pin of the `lit` count (13)** instead of `>=` — catches lexer regressions more strongly.

## Changed files

- `fixtures/duplication/normalization/` — 5 fixtures + ATTRIBUTION.md (commit 2ff4522)
- `tests/duplication_pmd_normalization_test.cpp` — 3 TEST_CASE (commit 2ff4522)
- `tests/duplication_renamed_recall_test.cpp` — P0.6 tradeoff pin (commit 1fbc9f4)

## Outcome
**Status:** completed
**Completion date:** 2026-06-11
