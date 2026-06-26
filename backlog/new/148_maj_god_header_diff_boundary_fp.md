# [BUG][RULES] `--diff` flags an already-god header as a "new god-header" (boundary FP)

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** new
**Module:** RULES / GRAPH / DIFF
**Priority:** major
**Related:** #126 (header/impl cycle FP — adjacent diff-precision class), #133 (god-header noise floor)

## Symptom (caught on the trending run #145)

`78/xiaozhi-esp32` produced 6 gate-fails / 300 commits (2%, versus ~0.1% for the corpus). Skeptic check:
all 6 are `new_god_headers`, and the headers were **already god (≥50 includers) in the PARENT**:

| commit | header | fan-in parent → child |
|---|---|---|
| 97c0e75 | `es8311_audio_codec.h` | **51 → 52** |
| d545f74 | `mcp_server.h` | **51 → 55** |
| 022d984 | `system_reset.h` | 50 → 51 (boundary) |

The commits added +1…+4 includers to an already-central header. This is not "the PR created a god-header"
(what the gate promises), but "the PR touched an already-god header". **False gate-fail** (exit 1 in CI).

(The count is a raw grep of `#include`; archcheck resolves into components, so the absolute number may differ
slightly, but "already god / on the boundary" is confirmed 3/3.)

## Probable cause

A "new god-header" in a diff should require `baseline_fanin < threshold AND current_fanin >=
threshold`. It looks like the baseline fan-in is computed unstably (include resolution of the base graph differs
slightly from the current one → a header that resolves to <50 components in base yields ≥50 in current)
→ a sporadic crossing of the threshold. Or the baseline god-set is not being subtracted at all.

## What to check / fix
- [ ] Reproduce on the 3 xiaozhi cases; print the baseline fan-in and current fan-in as archcheck sees them (not the grep).
- [ ] Confirm that "new god-header" = strictly crossed the threshold (was <thr, became ≥thr), not "god in current".
- [ ] If the cause is instability of base vs current resolution: compute fan-in on a single basis of resolvable edges.
- [ ] Fixture: header with fan-in 51 in both snapshots + a commit with +1 includer → NOT a gate-fail.
