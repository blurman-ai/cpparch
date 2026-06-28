# [report] `--diff` PR-comment polish — markdown summary, code-fence, clickable findings

**Created:** 2026-06-28
**Start date:** 2026-06-28
**Completed:** 2026-06-28
**Status:** completed (shipped in v0.1.3–v0.1.5: --format=md, both spans, both-side clickable links)
**Module:** report
**Priority:** minor
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** #156 (new-clone demo — surfaced the raw-comment gap), #123 (new-clone gate), example PR workflow

## Goal

Make the `archcheck --diff` output read well **as a GitHub PR comment**, not just as a
terminal dump. Today the sticky comment is the raw text report (`key: value` lines + a flat
`file:line: RULE — message` list). It works, but next to SonarQube's PR decoration it looks
primitive, and GitHub markdown can mangle the unfenced text (collapsed indentation / stray
formatting). This is polish to do **when we next touch the demo** (#156) — not urgent.

## Context

Comparison done under #156 (2026-06-28): archcheck's **inline `::warning::` annotations are
already industry-standard** (same shape as cppcheck/lizard-in-CI / reviewdog). The gap is the
**summary comment**. cppcheck/lizard/NiCad post no PR comment at all; SonarQube posts a polished
markdown summary (quality-gate badge, counts by severity, links). archcheck sits in between —
it has a comment, but it's raw.

Decide first **where** this lives:
- **(a) a GitHub-flavoured reporter mode** in archcheck itself (e.g. `--format=md` / `--github`)
  that emits markdown — reusable by every user, testable with fixtures (preferred, matches the
  "one rule = fixtures" bar); or
- **(b) just dress it up in the demo workflow** (wrap in a code-fence + a jq/shell-built header)
  — zero tool change, but every user re-invents it.

Lean (a) for a real product surface; (b) is the cheap stopgap the demo can carry meanwhile.

## Execution plan

- [x] Decide (a) tool-side `--format=md` vs (b) workflow-side — chose **(a)**.
- [x] **Code-fence** the raw report block (`writeTextReport` wrapped in ` ``` `).
- [x] **One-line summary header** with gate emoji + primary violation count.
- [x] `text`/`json` unchanged; `md` added; unit tests pinned (pass/advisory/fail + link states).
- [x] Demo workflow updated to use `--format=md` for the sticky PR comment.
- [x] **Clickable findings** — DONE: md now renders the advisory findings as a bullet list, and
      each `file:line` links to that line at the head commit (`{GITHUB_SERVER_URL}/{GITHUB_REPOSITORY}/blob/<currentSha>/file#Lline`).
      Context comes from the env GitHub Actions always sets; off-CI it falls back to plain code spans.
- [x] **`<details>` collapse** — DONE: structural metadata folded into `<details>`, findings on top.

## Done

- `include/archcheck/diff/md_report.h` + `src/diff/md_report.cpp` — `writeMdReport()`
- `OutputFormat::Markdown` in enum; `--format=md` accepted by `--diff`
- 3 unit tests in `tests/unit/diff/regression_report_test.cpp` (tag `[md_report]`)
- Demo workflow `archcheck-pr.yml` updated: markdown → PR comment, text → step summary

## Next steps

- Ready to close after v0.1.3 ships and the demo is re-run on it (the demo workflow uses
  `--format=md`, so PR comments show the summary + clickable findings).
- Possible v0.2 nicety (not blocking): link the *source* span (`clone of <file>:<a>-<b>`) too,
  and switch from blob links to PR-diff-anchor links when the diff anchor is derivable.

## Key decisions

| Decision | Reason |
|---------|---------|
| tool-side `--format=md`, not workflow dressing | reusable by any user; testable; matches "reporter = one file" pattern |
| polish the comment, not the inline annotations | annotations already match the industry standard (#156 comparison); the comment is the weak surface |
| advisory-only framing in the header | matches shipped behaviour — `DRIFT.NEW_CLONE` does not gate (v0.2 decision #103/#124) |
| clickable findings deferred | tool has no access to GitHub context at runtime |

## Commit

`7f8820f` feat(report): add --format=md for --diff; markdown PR comment (#157)

## Changed files

- `include/archcheck/diff/md_report.h` (new)
- `src/diff/md_report.cpp` (new)
- `src/cli/check_command.h` — `OutputFormat::Markdown` added
- `src/main.cpp` — `--format=md` parsed
- `src/cli/diff_command.cpp` — Markdown branch in `runDiffFullPath`
- `src/CMakeLists.txt` — `md_report.cpp` registered
- `tests/unit/diff/regression_report_test.cpp` — 3 `[md_report]` tests added
- `tests/unit/scan/new_clone_drift_test.cpp` — pre-existing clang-format fixed
