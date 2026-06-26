# [CONFIG][DIFF] diff_max_added_lines threshold: silence LCX advisory on bulk imports

**Date created:** 2026-06-12
**Date started:** 2026-06-12
**Status:** completed (duplicate — implemented under #117 due to an ID clash)
**Date completed:** 2026-06-12 (feature), closed as duplicate 2026-06-13
**Module:** CONFIG / CLI(diff)
**Priority:** minor
**Difficulty:** small
**Related:** #109 (corpus: mega-drops yield hundreds of mechanically-honest findings)

> **DUPLICATE RESOLUTION (2026-06-13):** this task and `completed/117_min_diff_max_added_lines.md`
> describe one feature — the `thresholds.diff_max_added_lines` threshold. The implementation went
> under number #117 (see that file: `collectDiffAdvisories` + `git::totalAddedLines` +
> table parser + skip-marker, verified on hpsx64 +419K). The whole DoD below is closed.
> Cross-checked against the code 2026-06-13: `config.h:diffMaxAddedLines=10000`,
> `config_loader.cpp` key `diff_max_added_lines`, `diff_command.cpp` gate + marker.
> The file is left in history as a trace of the #117/#118 ID clash.

## Goal

"Mega-drop" commits (bulk import of third-party sources: "extract sources" +419K
lines → 241 findings; vsomeip +21.9K → 19) — are not authorial code evolution, their
LCX signals are noise by volume. By default the config introduces a maximum of
added diff lines; on exceeding it the local-complexity advisory is
skipped with an explicit marker. The threshold is overridable in `.archcheck.yml`.

## Decisions

- Default **10,000 added lines**: calibrated against corpus #109 — honest
  large features are below (sbbs SQLite class +5.6K, webf +1.6K), bulk drops above
  (vsomeip +21.9K, hpsx64 +419K, nvdajp +3.37M).
- Key `thresholds: diff_max_added_lines` (the same phase-2 section as
  chain_length / god_header_fan_in).
- Added lines counted via `git diff --numstat` (already collected for
  test-co-evolution — zero new git calls).
- Silences ONLY the complexity advisory (SATD/test-co-evolution stay):
  the skip is printed explicitly, doesn't vanish silently.

## Definition of Done

- Thresholds.diffMaxAddedLines + key parser + pass/fail fixtures.
- Added-line summer — a testable helper (unit test).
- Skip with a marker in the --diff text output.
- Gates green, dogfooding clean.
