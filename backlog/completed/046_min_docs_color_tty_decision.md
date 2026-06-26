# [DOCS][REPORT] Color TTY output: implement or remove from roadmap

**Date created:** 2026-05-29
**Date started:** 2026-06-11 (Haiku)
**Status:** completed (Haiku 2026-06-11)
**Assignee:** Haiku
**Module:** DOCS / REPORT
**Priority:** minor
**Complexity:** XS (≤ 2 hours either way)
**Blocks:** —
**Blocked by:** —
**Related:** #6 (gh — audit Issue 8), #045 (docs_sync_roadmap — the roadmap decision logically belongs here too)

## Goal

`docs/architecture-spec.md` v0.1 roadmap promises "text report with color output in TTY", but `src/report/text_reporter.cpp` does no `isatty`/ANSI handling. The fork: either implement it (isatty-gated ANSI + `NO_COLOR`), or remove the promise from the roadmap.

## Context

A minor doc↔code discrepancy. Colors are nice, but not critical, especially in CI (where `isatty` = false and highlighting would be off anyway). The implementation is a few lines. Removing it from the spec is one line.

## Execution plan

Decide first, then act. One of two tracks:

### Track A: implement
- [ ] Check `isatty(fileno(stdout))`; if not a TTY — no color
- [ ] Respect the `NO_COLOR` env (https://no-color.org/) — if set to any non-empty value, no color
- [ ] ANSI escape for severity (error=red, warning=yellow, note=cyan) — minimal set
- [ ] Test: compare stdout without a TTY (PIPE) — must be clean text with no escape codes

### Track B: remove from roadmap
- [ ] Delete the line about color in TTY from the v0.1 section of `docs/architecture-spec.md`
- [ ] (optionally) Move it to v0.3+ as a nice-to-have

## Acceptance criterion

Spec and behavior agreed — either color in TTY works and is tested, or the promise is removed from the spec.

## Done

- [x] Track A fully implemented (2026-06-11)
- [x] Added parameter `bool useColor = false` to `writeTextReport()`
- [x] Implemented ANSI logic: red for `[ruleId]` and summary, green for success
- [x] Integration in `main.cpp`: `isatty()` + `NO_COLOR` env check
- [x] Full set of unit tests (4 control cases) in `text_reporter_test.cpp`
- [x] All 415 tests pass without changing expectations
- [x] Live pipe: 0 ANSI codes (isatty=false in a pipe → useColor=false)
- [x] Lizard: 0 warnings on the new code
- [x] Dogfood: 0 violations of own rules
- [x] Diff: 37 lines (< 50), 1 new file (< 2)

## In progress

- (empty — task complete)

## Next steps

1. ✅ Decided (2026-06-11): **Track A — implement**. Track B closed.
2. ~~fmt~~ — NOT applicable: fmt is not part of the frozen stack (C++20, ryml, Catch2 only). We write ANSI codes by hand.

## Plan for Haiku (2026-06-11)

Before starting, MUST read in full: this task, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2, [docs/code_style.md](../../docs/code_style.md), [docs/code_quality.md](../../docs/code_quality.md).

### Allowed forks (facts, verified 2026-06-11)

- Track = **A** (implement). Do NOT touch the spec line `docs/architecture-spec.md:630` — after implementation it becomes true.
- `rules::Violation` (include/archcheck/rules/violation.h) **has no severity field** — only `ruleId/file/line/message`. Do NOT do "by severity" colors, do NOT add the field. We color: `[ruleId]` — red, the summary line `N violation(s) (...)` — red, `No violations found.` — green.
- The repo has TWO `writeTextReport` functions: ours — `src/report/text_reporter.cpp:31`; another — `archcheck::diff::writeTextReport` in `src/diff/regression_report.cpp:254`. Do **NOT touch** the diff reporter.
- The codebase is already POSIX-only (`src/git/git_exec.cpp` uses fork/exec) — `::isatty(fileno(stdout))` from `<unistd.h>` without Windows guards.
- There is NO test file for the text reporter (in `tests/unit/report/` only `json_escape_test.cpp` and `violation_baseline_test.cpp`) — create `tests/unit/report/text_reporter_test.cpp` (1 new file, within limit) and register it in `tests/CMakeLists.txt`.

### Design (must be exactly this)

1. Signature: `writeTextReport(const rules::ViolationList&, std::ostream&, bool useColor = false)` — default `false`, so that all existing calls and tests stay valid without edits.
2. ANSI: red `\033[31m`, green `\033[32m`, reset `\033[0m`. No other codes, no "growth" palette.
3. The color decision — in `src/main.cpp:142` (the only call to our reporter): `useColor = ::isatty(fileno(stdout)) != 0 && !no_color_set`, where `no_color_set` = `std::getenv("NO_COLOR")` is a non-empty string (https://no-color.org: any non-empty value disables color).

### Planned files (only these)

| File | Change |
|------|-----------|
| `include/archcheck/report/text_reporter.h` | 3rd parameter `bool useColor = false` |
| `src/report/text_reporter.cpp` | color wrappers around `[ruleId]` / summary / "No violations found." |
| `src/main.cpp` | isatty + NO_COLOR → `useColor` in the call at line 142 |
| `tests/unit/report/text_reporter_test.cpp` | new, 4 cases (below) |
| `tests/CMakeLists.txt` | registration of the new test |

### Control cases (contract)

| Case | Expectation | Result |
|------|----------|-----------|
| 1 violation, `useColor=false` | **0** occurrences of `\033` in the output | ✓ PASS (assert escapeCount == 0) |
| 1 violation, `useColor=true` | output contains `\033[31m[` and `\033[0m` | ✓ PASS (find() != npos) |
| empty list, `useColor=true` | output contains `\033[32mNo violations found.\033[0m` | ✓ PASS (find() != npos) |
| call via the old 2-argument form | compiles, output byte-for-byte as before the change | ✓ PASS (backward compat test) |

### Definition of done

- All existing tests green **without editing their expectations** (411+ as of 2026-06-11).
- 4 control cases green.
- Live pipe check: `~/projects/cpparch/build/debug/src/archcheck ~/projects/cpparch/src | grep -c $'\033'` → `0`.
- lizard 0 warnings; dogfood 0 violations (`src/ include/ tests/`).
- Fit within ≤50 lines of diff (excluding the test), ≤2 new files.

### Do not do

- Do NOT touch `diff::writeTextReport`, `json_reporter`, `rules::Violation`, the spec, the README.
- Do NOT add a `--color` CLI flag — not requested (YAGNI).
- Do NOT commit without an explicit command.

### Escalation (when to stop and hand off to a senior model)

Stop, write "Blocked: <what/why/what you tried>" here, and report if: you find a contradiction between the task and the code; a test fails after 2 honest attempts to fix the CODE (not the test); you need a file outside the table above; the diff does not fit the limits. After that, the task continues with Sonnet, then Opus. Do NOT bend tests to fit. 

## Key decisions

| Decision | Reason |
|---------|---------|
| Create a separate task, do not mix into #045 | different kind of work: #045 — rewriting texts, here — either implement or delete one line |

## Changed files

| File | Change |
|------|-----------|
| `src/report/text_reporter.cpp` | (Track A) isatty + ANSI |
| `docs/architecture-spec.md` | (Track B) remove TTY-color from v0.1 |
| `tests/...` | (Track A) test pipe → no ANSI |
