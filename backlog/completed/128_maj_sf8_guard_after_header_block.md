# [RULES][SF.8] guard detector misses `#ifndef` after a long header comment

**Created:** 2026-06-14
**Started:** 2026-06-19
**Completed:** 2026-06-19
**Status:** done
**Module:** RULES / SCAN
**Priority:** major
**Difficulty:** quick_win

## Done (2026-06-19)

The root cause was in `sf8_include_guard.cpp`: `hasIncludeGuard`/`isObjcFile` counted
the first `kScanLines=60` **non-empty** lines, including comments, so the nanovdb
guard on line 125 (after a ~122-line header) was not found — the budget was
exhausted on the license. Any fixed line ceiling does not solve the bug.

Fix: added `stripComment` (block-comment-aware, `/* */` state carried across
lines) + a shared `scanLeadingCode`; now the budget counts only lines of **code**,
and the header length no longer matters. `closesGuardPair` stores the names as `std::string`
(the scanned line is a temporary buffer). Self-contained, include_scanner not touched.

- Tests: +3 unit cases (guard under a 100-line block banner; `#pragma once` under
  a 100-line `//` banner; long banner without a guard → still fires). 540/540 green.
- Dogfood `src/ include/ tests/` → 0 violations.
- Cross-check with the binary: `nano.h` (guard on line 123) is not flagged,
  `bare.h` (without a guard) fires. Not just the aggregate — specific cases by hand.
- Fixture contract: `fixtures/sf8_include_guard/pass/guard_below_block_banner.h`.
**Blocks:** —
**Blocked by:** —
**Related:** goodfirstissue dogfood (openvdb), #127 (vendor — adjacent, but this is a SEPARATE bug about our own code)

## Goal

SF.8 (header missing `#pragma once`/include guard) gives a false positive when
the include-guard `#ifndef X` is **not at the start of the file, but after a long license/
doc block**. The detector apparently looks for the guard only in the "head" of the file.

## Evidence (confirmed by recheck, 2026-06-14)

Run of archcheck over **openvdb**: SF.8 ×30, ALL in `nanovdb/`. Rechecked the files
by hand (a separate clone):

| File | archcheck says | Actually |
|---|---|---|
| `nanovdb/nanovdb/NanoVDB.h` | missing guard | `#ifndef NANOVDB_NANOVDB_H_HAS_BEEN_INCLUDED` on **line 125** |
| `nanovdb/nanovdb/HostBuffer.h` | missing guard | guard on **line 77** |

Both have a guard, but after ~75-125 lines of license/documentation. archcheck does not
find it → a false "missing". nanovdb is part of OpenVDB itself (OUR code), not vendor.

## Hypothesis about the cause (verify against the code)

The SF.8 detector (`src/rules/sf8_*` or the scan layer) probably:
- reads only the first N lines/bytes of the file, OR
- expects `#ifndef`/`#pragma once` as the first significant directive and gives up upon meeting
  anything else (although it should skip comments).

Check in the code where exactly the guard search aborts. Find the rule file via
`grep -rl 'pragma once' src/rules src/scan`.

## How to fix (sketch)

Scan for the guard over the **whole file** (or at least up to the first non-comment/non-pragma
significant line of code), not a fixed head. Correctly skip:
- leading block `/* ... */` and line `//` comments of any length;
- empty lines;
- then the first `#pragma once` OR a paired `#ifndef X` / `#define X` (the classic guard).
Cost: reading a bit further into the file; for large files it can be capped at a reasonable
ceiling (e.g. up to the first `#include` or the first non-pp line of code).

## Note on self-checking (lesson of this finding)

A cluster of 30 identical SF.8s in one subfolder LOOKED like a TP at first (by the
"selectivity" argument — if not everything is flagged, the detector works). That reasoning
MISLED here: what was selective was not the rule's correctness but nanovdb's uniform style (guard
after the header). Only a manual recheck of the file saved it. Lesson for the backlog: a cluster of
identical SF.8s in one subtree → check the guard in the file itself, do not trust the aggregate.

## Verification (fixtures are mandatory)

- [x] guard under a large `/* license */` block → 0 (unit + fixture
      `pass/guard_below_block_banner.h`)
- [x] `#pragma once` after a `//` header → 0 (unit case)
- [x] genuinely without a guard → fires, not over-suppressed (unit case "long banner with no guard")
- [ ] regression on a real clone of openvdb nanovdb (30 → ~0) — moved to #131
      (fresh golden measurement); locally confirmed with synthetics shaped like nanovdb

## Changed files

- `src/rules/sf8_include_guard.cpp` — `stripComment` + `scanLeadingCode`; the budget
  counts lines of code, not comments; `closesGuardPair` owns the names (commit `9f247a6`)
- `tests/unit/rules/sf8_include_guard_test.cpp` — +3 cases
- `fixtures/sf8_include_guard/pass/guard_below_block_banner.h` — fixture contract

## Outcome

**Status:** completed. The bug (30 FPs on openvdb/nanovdb) eliminated at the root: any
fixed line ceiling broke on a long header — now comments are stripped and do not burn the
budget. The binary cross-check confirmed it. The regression on a real openvdb clone —
in #131.
