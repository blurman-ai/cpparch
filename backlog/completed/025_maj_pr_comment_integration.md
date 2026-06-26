# [CI][DOC] PR visibility of the report: sticky-comment + Check annotations + merge_group in the example workflow

**Date created:** 2026-05-27
**Date started:** 2026-05-28
**Date completed:** 2026-05-28
**Status:** completed
**Module:** CI, DOC
**Priority:** major
**Complexity:** S (1-2 hours)
**Blocks:** —
**Blocked by:** — (#018 provides the text report; that's enough)
**Related:** #018 (git_diff_analysis), [docs/ci_integration.md](docs/ci_integration.md)

## Goal

Turn [.github/workflows/example_archcheck_pr.yml](.github/workflows/example_archcheck_pr.yml)
from "here is an artifact with the report" into a **reference** CI snippet that
shows the result right in the PR UI without a download-artifact step:

1. archcheck-output → `$GITHUB_STEP_SUMMARY` (visible with a single click from
   the run page).
2. archcheck-output → sticky PR-comment via
   [marocchino/sticky-pull-request-comment](https://github.com/marocchino/sticky-pull-request-comment)
   (visible right in the PR conversation tab; update-in-place on pushes).
3. Workflow triggers **both** on `pull_request` **and** on `merge_group` —
   otherwise the merge queue hangs waiting for the check ([docs.github.com — merge
   queue](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-a-merge-queue)).
4. Hide from draft PRs via `if: github.event.pull_request.draft == false`
   (optional).

## Context

The current example simply uploads-artifact (1 line `tee archcheck-diff.txt`
+ upload-step). To see the report, the reviewer has to open the Actions tab,
find the run, download the zip, unpack it. The UX is nonexistent.

Industry research (see the research-agent report 2026-05-27 in history):

- **Sonar PR decoration** — Quality Gate as a named Check, not a comment.
- **dependency-cruiser** — `depcruise-pr-check` action, draws a mermaid graph
  of changed dependencies right in the PR comment.
- **reviewdog taxonomy** — `github-pr-check` (Checks tab, file-level)
  fits an architectural report; `github-pr-review` (per-line) is
  fundamentally unsuitable, because a new cycle A→B→C→A can appear from a
  merge-from-main without edits in the lines themselves.
- **ArchUnit FreezingArchRule** — a baseline file, not a comment; orthogonal.

Sticky-comment is the de facto standard for tool-output in a PR (CodeClimate,
size-limit-action, bundlewatch). `marocchino/sticky-pull-request-comment`
uses `header:` as the identifier for update-in-place and gets by with
`GITHUB_TOKEN` without a PAT.

archcheck itself **must not** call the GitHub API — that would break the
git-host-agnostic property. All publishing is on the CI-step side.

## Execution plan

- [x] Extend `example_archcheck_pr.yml`:
  - [x] Triggers `pull_request` + `merge_group` with the same job-name (for the merge queue).
  - [x] Step: capture archcheck output to a file + tee to stdout.
  - [x] Step: append `archcheck-diff.txt` to `$GITHUB_STEP_SUMMARY` via `cat archcheck-diff.txt >> "$GITHUB_STEP_SUMMARY"`.
  - [x] Step: post a sticky-comment via `marocchino/sticky-pull-request-comment@v2` with `header: archcheck-diff` and `recreate: false`. Skip on `merge_group` event.
  - [x] `if: always()` on the comment-step, so regressions (exit 1) also land in the comment.
  - [x] `permissions: pull-requests: write` at the job level (minimum for sticky-comment).
- [x] Update the example_workflow header with an explanation of which three report-display channels it uses.
- [x] In [docs/ci_integration.md](docs/ci_integration.md) §"Publishing channels in a PR" §2 — add a direct `uses:` snippet.
- [x] Smoke: PR `smoke/025-sticky-comment-test` — the sticky-comment appeared in the conversation tab, all 6 CI checks green. PR closed.

## What is NOT part of this task

- **Annotations with file:line** (`::warning file=...,line=...`). Right now
  `EdgeRef` carries no location metadata; the real file:line will arrive
  when the scanner starts writing coordinates into the edge — a separate task.
- **SARIF reporter.** The third channel from docs §"Publishing channels";
  v0.2 roadmap.
- **Distribution as a marketplace composite-action** (`uses: archcheck/archcheck-action@v1`).
  This requires a pre-built binary and a release pipeline — a separate story
  closer to v0.2.

## Done

- `example_archcheck_pr.yml` fully rewritten: `merge_group` trigger,
  `permissions: pull-requests: write`, `$GITHUB_STEP_SUMMARY`, sticky-comment
  via `marocchino/sticky-pull-request-comment@v2`. The
  `continue-on-error: true` + final fail-step pattern — the report is published
  even on a regression.
- `docs/ci_integration.md` §2 "Sticky PR comment" — added a full ready-made
  snippet explaining each field.
- Local smoke test: `archcheck --diff master..HEAD` on a branch with `fixtures/smoke_025/`
  gives `added_edges: 1`, `exit=1` — the output is correct for the sticky-comment.
- GCC 13 bug: `(void)::read()` does not suppress `-Wunused-result` in a Release build with `-Werror`.
  Fix: `[[maybe_unused]] auto n = ::read(...)` (commit `d5d31db`).
  All 222 tests green.
- Smoke PR `smoke/025-sticky-comment-test` opened on GitHub; the second CI run
  (with the fix) is in progress.

## In progress

—

## Next steps

—

## How it works (for documentation)

Three channels of report visibility in a GitHub PR:
1. **Step Summary** — `cat archcheck-diff.txt >> "$GITHUB_STEP_SUMMARY"`, available from the run page with a single click.
2. **Sticky comment** — `marocchino/sticky-pull-request-comment@v2` with `header: archcheck-diff`, updated in place on force-push, doesn't spam the feed.
3. **Failed check** — the job fails with exit 1 on a regression, the red Check automatically blocks the merge.

The `continue-on-error: true` + final fail-step pattern allows the report to be published even on a regression.
`if: github.event_name == 'pull_request'` on the comment-step — the sticky-comment is skipped for `merge_group` (there is no PR there to comment on).

Incidental fixes in the smoke PR:
- `(void)::read()` → `[[maybe_unused]] auto n = ::read()` — GCC 13 with `-Werror=unused-result` rejects the `(void)`-cast for `read()`.
- `path.find(prefix) == 0` + `// cppcheck-suppress stlIfStrFind` — `starts_with` unavailable on GCC 8 (Astra Linux 1.7).

## Key decisions

| Decision | Reason |
|---------|---------|
| Sticky-comment (not a proliferating one) | A force-push to the PR branch must not spam the discussion feed. The header key `archcheck-diff` ensures update-in-place |
| `marocchino/sticky-pull-request-comment` as a named-dependency | De facto standard (CodeClimate, size-limit, bundlewatch use it); gets by with `GITHUB_TOKEN` without a PAT |
| `merge_group` alongside `pull_request` | Without it, the GitHub Merge Queue blocks on a missing check. The cost — one extra trigger, the profit — merge queue support out of the box |
| `$GITHUB_STEP_SUMMARY` plus sticky-comment | Step Summary — for the read-only reviewer who opens the run; sticky-comment — for the PR thread. Not duplication, different audiences |
| archcheck doesn't publish itself | Preserves git-host-agnostic (GitLab/Bitbucket/Gitea/self-hosted work without modifying the tool). Publishing — an adapter-step in the YAML |
| `permissions: pull-requests: write` explicitly | GitHub-default workflow permissions since autumn 2024 — read-only; without an explicit grant the sticky-comment won't be written |
| `[[maybe_unused]] auto n = ::read(...)` instead of `(void)::read(...)` | GCC 13 (-Werror=unused-result) rejects the `(void)`-cast for `read()`; `[[maybe_unused]]` — a C++17 idiom, works everywhere |

## Changed files

| File | Commit | Change |
|------|--------|--------|
| `.github/workflows/example_archcheck_pr.yml` | prev session | extension: triggers, step-summary, sticky-comment, permissions |
| `docs/ci_integration.md` | prev session | §"Sticky PR comment" — ready-made snippet with `marocchino/...` |
| `src/git/git_object_file_source.cpp` | `d5d31db` | GCC 13 fix: `(void)::read` → `[[maybe_unused]] auto n` |
| `fixtures/smoke_025/a.h`, `b.h` | `f4e8389` | smoke fixture for PR verification (temporary) |
