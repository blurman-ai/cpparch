# [SCAN] UTF-8 BOM before the first `#include` is not recognized, breaks DRIFT.1

**Created:** 2026-05-29
**Started:** 2026-05-29
**Completed:** 2026-05-29
**Status:** done
**Module:** SCAN/include_scanner
**Priority:** critical (produces a false-positive in DRIFT and, probably, in SF.8)
**Complexity:** S (≤ 15 min: strip BOM at the start of scanning a file)
**Target release:** v0.1
**Blocks:** reliability of DRIFT.1 on real repos
**Related:** #033 (ai_drift_dataset)

## Symptom

A DRIFT.1 run on BambuStudio PR #10794 (2026-05-29, see milestones.md run 13)
produced a false positive:

```
slic3r/GUI/MsgDialog.cpp: [DRIFT.1] shortcut edge: slic3r/GUI/MsgDialog.cpp -> slic3r/GUI/MsgDialog.hpp
```

Checked manually: `MsgDialog.cpp` in **both** revisions starts with `#include "MsgDialog.hpp"`.

## Cause

The file in the before-revision has a **UTF-8 BOM** at the start:

```bash
$ git show 2263815...:src/slic3r/GUI/MsgDialog.cpp | head -1 | od -c | head -1
0000000 357 273 277   #   i   n   c   l   u   d   e       "   M   s   g
        \---BOM---/
```

In the after-revision the BOM was removed. Our `include_scanner` doesn't strip the BOM
before the `#include` regular expression → the first line with the BOM doesn't match
→ include "MsgDialog.hpp" is missed in the baseline → in the new version the same
include is seen as a "new edge" → DRIFT.1 false-positive.

## Impact

1. **DRIFT.1 / DRIFT.2** lose trust on real repositories — Windows-style
   files with a BOM are common (especially in legacy C++ projects,
   wxWidgets-based applications, MSVC-generated files).
2. **SF.8** (no_include_guard): the same bug should affect SF.8 — `#pragma once`
   on the first line with a BOM is skipped → false "no guard".
3. **The dependency graph** as a whole loses edges for BOM files.

## Fix

In `include_scanner::scan()` (or `scan_text()`) — at the start of the file:

```cpp
constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
if (content.starts_with(kUtf8Bom))
    content.remove_prefix(kUtf8Bom.size());
```

Likewise check the UTF-16 BOM (`FF FE` / `FE FF`) and UTF-32 — but those
are much rarer in C++ sources.

## Fixture

`fixtures/scan/utf8_bom/`:
- `with_bom.h` — file starts with a BOM + `#pragma once` + `#include "other.h"`
- `without_bom.h` — the same content without a BOM
- Test: both must produce the same set of includes and the same SF.8 result.

## Done

- Bug found on BambuStudio PR #10794 (see `/tmp/bambustudio_drift.txt`).
- UTF-8 BOM strip added at the start of `scanIncludes()` in
  [src/scan/include_scanner.cpp](../../src/scan/include_scanner.cpp).
  Implemented via `string_view::compare` (not `starts_with`) —
  `GCC8-COMPAT`, libstdc++ 8 doesn't have the method.
- Unit tests added: 3 cases in `[scan][scanner][bom]` —
  (1) strip before the first `#include`, (2) equivalence of the result
  with/without BOM, (3) embedded BOM in the middle is not counted as first-significant.
- Fixtures `fixtures/scan/utf8_bom/{with_bom.h, without_bom.h, other.h, README.md}`
  for documentation and future integration tests.
- **SF.8 checked and turned out to be immune to the BOM**: `hasPragmaOnce`/
  `hasIfndefGuard` use a substring `find()`, which finds the
  keyword even after a BOM. Nonetheless, 2 regression
  cases were added in `[rules][sf8][bom]` — in case someone rewrites
  the predicates via `starts_with`.
- Full run: `archcheck_tests` 229 case / 754 assertions — green.
- Lizard: `--CCN 15 --length 30 --arguments 5 --warnings_only` — clean.

## Changed files

- `src/scan/include_scanner.cpp` — strip BOM in `scanIncludes()`.
- `tests/unit/scan/include_scanner_test.cpp` — +3 BOM tests.
- `tests/unit/rules/sf8_include_guard_test.cpp` — +2 regression BOM tests.
- `fixtures/scan/utf8_bom/` — new fixture.

## In progress

- (empty)

## Next steps

1. ~~Strip BOM in `include_scanner`~~ ✅
2. ~~Fixture `fixtures/scan/utf8_bom/`~~ ✅
3. ~~Re-run DRIFT on BambuStudio PR #10794~~ ✅
   **2026-05-29 re-run on a fresh clone `~/oss/BambuStudio`** (see
   milestones.md "Run 14 — DRIFT re-run after the BOM fix"): DRIFT.1 3 → **2**.
   The FP `MsgDialog.cpp -> MsgDialog.hpp` disappeared, the two real hits remained.
4. Recheck SF.8 reports from earlier milestones (abseil, folly) —
   whether there are hidden BOMs there, formally not needed after the fix, but useful
   for confidence. (Low priority — the substring-find SF.8 was robust.)
