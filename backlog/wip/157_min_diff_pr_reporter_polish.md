# [report] `--diff` PR-comment polish — markdown summary, code-fence, clickable findings

**Created:** 2026-06-28
**Start date:** 2026-06-28
**Status:** wip
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
- [x] `text`/`json` unchanged; `md` added; 3 unit tests pinned (pass/advisory/fail states).
- [x] Demo workflow updated to use `--format=md` for the sticky PR comment.
- [ ] **Clickable findings** — deferred; needs GitHub context (repo URL + head SHA) not available to the tool.
- [ ] Optional: `<details>` collapse — may add in v0.2 when the output grows.

## Done

- `include/archcheck/diff/md_report.h` + `src/diff/md_report.cpp` — `writeMdReport()`
- `OutputFormat::Markdown` in enum; `--format=md` accepted by `--diff`
- 3 unit tests in `tests/unit/diff/regression_report_test.cpp` (tag `[md_report]`)
- Demo workflow `archcheck-pr.yml` updated: markdown → PR comment, text → step summary

## Next steps

1. Pick (a) vs (b).
2. If (a), add the reporter alongside `text_reporter`/`json_reporter` (one file + factory line);
   if (b), edit `experiments/clone_gate_demo_156/templates/archcheck-pr.yml`.

## Key decisions

| Decision | Reason |
|---------|---------|
| polish the comment, not the inline annotations | annotations already match the industry standard (#156 comparison); the comment is the weak surface |
| advisory-only framing in the header | matches shipped behaviour — `DRIFT.NEW_CLONE` does not gate (v0.2 decision #103/#124) |

## Fixtures (if a rule)

- n/a — a reporter, not a rule. If done tool-side (a), pin the rendered markdown with reporter
  fixtures (0-finding pass + ≥1-finding fail).
