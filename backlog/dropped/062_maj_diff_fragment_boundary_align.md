# [SCAN] #056: align fragments to function boundaries (FP-segmentation)

**Created:** 2026-05-31
**Status:** dropped (absorbed by the #072 design)
**Module:** SCAN
**Priority:** major
**Related:** #060, #056

## Goal
Remove FP-segmentation (~15% in iter1): a fragment cuts across functions
(`tail of func_A + head of func_B`) and matches an unrelated piece.

## Fix idea
Fragmentation must not emit blocks crossing the boundary of a function/top-level
block. A candidate fragment = a whole function/body (by `{}` balance from the signature),
not an arbitrary window. Cut off pairs where ADDED or BASE starts/ends
in the middle of a function.

## Verification
- [ ] iter-N: the FP-segmentation share drops, TPs are not lost.

## Outcome

**Status:** dropped — no separate work needed, the goal is achieved by another task's design.
**Closing date:** 2026-06-11 (backlog review).

FP-segmentation was an artifact of the line-window segmentation of the **spike** #053/#056 (arbitrary line
windows). The production fragmenter, written in #072 (`src/scan/duplication/fragmenter.cpp`),
cuts along function boundaries from the start: a block is emitted only for a `{` preceded by a `)`
(a function/control body), and the cap of 600 tokens (#091) keeps ~120-line functions whole. The FP class
"tail of func_A + head of func_B" is impossible by construction in the production pass — the fix proposed
here is implemented in #072 as a property of the design, not as a separate change.
