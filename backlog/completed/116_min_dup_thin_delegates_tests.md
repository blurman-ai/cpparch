# [SCAN][DUP] Tests: an interface + thin delegating implementations are not counted as copy-paste

**Created:** 2026-06-12
**Started:** —
**Status:** new
**Module:** SCAN/duplication
**Priority:** minor
**Difficulty:** small
**Assignee:** suitable for Haiku (fixture + Catch2 tests from a ready specification)
**Blocks:** section 9.1 of docs/duplication_architecture.md (marked "not verified")
**Blocked by:** —
**Related:** #109 (where the question arose), #056 (token detector), #059 (precision layer)

## Goal

Directly test the hypothesis from docs/duplication_architecture.md §9.1: the typical
polymorphic pattern "interface + similar thin implementations" is not reported as a
duplicate, while genuine copy-paste of substantive bodies is reported.

## Setup (from the user, 2026-06-12)

A test on an interface implementation of **10 methods with a different number of arguments in
each method** (arity 0..9), the implementations being **one or two lines** (delegation
to a backend and exit). Several implementation classes of one interface.

## Scenarios

1. **Interface of 10 methods × 3 implementations, delegating to different backends** —
   expectation: 0 pairs (bodies < minTokens=30 are not fragmented; even if they were
   fragmented — the callees differ).
2. **The same implementations, but delegating to ONE backend** (the only difference being the
   class name) — expectation: 0 pairs (still below minTokens).
3. **Control-TP: two substantive bodies (>30 tokens, common callees)** in two
   implementations — expectation: a pair is reported. Note: a corpus of 2 fragments
   is degenerate for the relative rare-df — the fixture must contain enough
   background (other functions) for rare tokens to exist.
4. **Boundary: a delegate bloated to ~30+ tokens** (long argument lists,
   10 parameters) — record the actual behavior and write it into §9.1.

## Form

- A fixture `fixtures/duplication/thin_delegates/` (pass scenarios 1–2,
  fail scenario 3) OR a unit test in `tests/` modeled on
  `duplication_fp_guards_test.cpp` — pick whichever is shorter; fixtures are
  preferred (CLAUDE.md: fixtures are mandatory).
- Run through `scanForDuplication` with default ScannerOptions.

## Definition of Done

- Tests green, all 4 scenarios recorded.
- §9.1 in docs/duplication_architecture.md rewritten: "⚠️ NOT VERIFIED" →
  "verified (#116)", with the actual result of boundary scenario 4.
- If scenario 1/2 suddenly produces pairs — that's a BUG in the precision layer: open
  a separate major task, keep the tests as regression (don't paper over it).

## Done (2026-06-13)

Implemented as the unit test `tests/duplication_thin_delegates_test.cpp` (4 cases, tag `[thin-delegates]`), not as a fixture — shorter and hermetic (modeled on `duplication_fp_guards_test.cpp`).

**Results of the 4 scenarios:**
1. ✅ Interface of 10 methods × 3 implementations, different backends → 0 pairs.
2. ✅ The same implementations, one backend (difference only in the class name) → 0 pairs.
3. ✅ Control-TP: two substantive `compute` bodies (>30 tokens) against a background of various functions → a pair is reported.
4. ✅ Boundary 10-parameter delegate → 0 pairs.

**Key finding (boundary scenario 4):** the fragmenter measures the size of the BODY, not the signature. A delegate with 10 parameters still yields a body `backend_.op9(a..j);` ≈25 tokens < minTokens=30 → 0 fragments. Bloating the parameter list does NOT push the delegate over the fragmentation threshold. This is recorded in §9.1.

**The task's warning about a degenerate corpus is confirmed:** 2 identical fragments with no background give `idf=log(2/2)=0` → weighted=0, and a genuine TP doesn't reach the gate. The TP fixtures (3–4) therefore carry various background functions ≥30 tokens — this is a requirement on the fixture, not a property of the detector.

§9.1 of docs/duplication_architecture.md rewritten: "⚠️ NOT VERIFIED" → "✅ verified (#116)". All tests green.
