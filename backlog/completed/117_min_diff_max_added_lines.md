
## How it works

`collectDiffAdvisories` sums the added lines of the already-collected numstat
(`git::totalAddedLines`); when `thresholds.diffMaxAddedLines` is exceeded, the
complexity advisory isn't computed, and an explicit line goes into the text output:
"skipped — diff adds N lines (bulk import; thresholds.diff_max_added_lines)".
SATD and test-co-evolution are not silenced.

## Key decisions

- Default 10,000: calibrated on the #109 corpus (honest features ≤ ~6K, drops ≥ ~20K).
- The thresholds parser switched to a table of member-pointers (3 keys, one path).
- Binary files in numstat (added = −1) don't decrease the sum (test).

## Changed files

- `include/archcheck/config/config.h` — Thresholds.diffMaxAddedLines = 10000
- `src/config/config_loader.cpp` — key diff_max_added_lines, table parser
- `include/archcheck/git/diff_query.h` — totalAddedLines()
- `src/cli/diff_command.cpp` — gate + skip marker + DiffConfig
- `fixtures/config/pass/thresholds/archcheck.yml`, loader/diff_query tests
- `docs/config_format.md` — key documented

## Outcome

**Status:** completed
**Completion date:** 2026-06-12
Verified on a real mega-drop hpsx64 (+419,797 lines): the advisory skipped with a
marker; an ordinary commit (+36 lines) is reported as before.
