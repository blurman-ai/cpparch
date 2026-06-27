# archcheck in CI: how it works on a PR

This document explains how archcheck is embedded in git-flow / GitHub Flow during
pull request review. **In short:** archcheck does not read a unified diff, does not
receive a patch blob, and does not parse the GitHub API. It is handed two git refs —
it materializes both states itself, computes the architectural delta, and separately
flags gating regressions and advisory signals.

## What archcheck considers a "diff"

archcheck does not work with a textual diff. It computes the difference of **two
dependency graphs**: "the project graph at baseline" vs "the project graph at
current". The delta is the edges added/removed in the graph and the cycles that
appeared/grew. This is the principal point: the tool answers the question "did this
PR worsen the architecture?", not "which lines changed".

A corollary: to anchor the delta, it needs **full snapshots of both states of the
source tree**, not a patch. That is why the interface is a git revspec, not a `.diff`
file.

### Two gating modes — `--diff` vs `--baseline`

These are different scenarios, not competitors:

| Mode                   | What it pins                                   | When                               |
|------------------------|------------------------------------------------|------------------------------------|
| `--diff <revspec>`     | regression of a specific PR                    | every PR run                       |
| `--baseline <file>`    | long-term technical debt (snapshot)            | release cuts, one-off refactors    |

The conceptual analog in the Java world is ArchUnit's `FreezingArchRule`
([ArchUnit docs §"Freezing arch rules"](https://www.archunit.org/userguide/html/000_Index.html)):
you "freeze" the list of existing violations into a file once, and afterward CI
reports only new ones. archcheck covers both patterns — the baseline file lives in
the repo next to the config, and `--diff` works on-the-fly without a file.

## CLI contract

```
archcheck --diff <revspec> [path]
```

`<revspec>` — a standard git revspec:

| Form           | Semantics                                                       |
|----------------|-----------------------------------------------------------------|
| `a..b`         | baseline = `a`, current = `b` (both git refs)                  |
| `<ref>`        | baseline = `<ref>`, current = the current working tree (WORKTREE) |

`path` — path to the project root inside the repository (default — cwd).
Support for `...` (three-dot) and empty sides is **rejected**, see
[parseRevspec](../include/archcheck/git/git_state.h#L25).

Exit codes are part of the contract (see [CLAUDE.md](../CLAUDE.md) §"Exit codes"):

- `0` — no gated regression. Advisory signals may still appear in the report.
- `1` — gated regression: new/grown cycle or new god-header. CI must fail.
- `2` — git/IO/config error, invalid revspec, not a git repo.

Removing edges is **not** counted as a regression — a PR that makes the architecture
stricter passes green even if the legacy project still has violations. Added edges,
chain/NCCD growth, SATD, test co-evolution, local complexity, flag-argument drift,
and new-clone drift are currently advisory: they are visible in the report but do
not break the job.

## What archcheck does under the hood

```
parseRevspec("origin/main..HEAD")
  → baseline="origin/main", current="HEAD"

materializeRef(baseline)
  → git worktree add --detach /tmp/archcheck-XXXX origin/main
  → buildGraphForPath(/tmp/archcheck-XXXX/path) → G_baseline

materializeRef(current)
  → if current == WORKTREE: use the working tree without a checkout
  → otherwise another worktree → G_current

buildRegressionReport(G_baseline, G_current)
  → gating: grownSccs, newGodHeaders
  → advisory: added/removed edges, chain/NCCD growth, hygiene drift

worktree remove --force /tmp/archcheck-XXXX  (RAII)
```

The tool **fork/exec**s `git` via `execvp` without a shell (no injection risk).
libgit2 was deliberately not pulled in — see the key decision in
[#018](../backlog/completed/018_crt_git_diff_analysis.md). Full implementation:
[git_state.cpp](../src/git/git_state.cpp), [graph_builder.cpp](../src/graph/graph_builder.cpp).

## Anatomy of a PR on GitHub / GitLab

In GitHub Actions terms:

| Variable                       | Meaning                                        |
|--------------------------------|------------------------------------------------|
| `github.base_ref`              | the PR's target branch (e.g. `main`)           |
| `github.head_ref`              | the PR's source branch                         |
| `HEAD` after checkout          | tip of the PR branch (or the merge commit under `merge_group`) |
| `origin/${{ github.base_ref }}` | tip of the target branch at the time of the CI run |

**Canonical revspec for a PR:** `origin/${{ github.base_ref }}..HEAD`.

The tool does not distinguish "merge-PR / squash-PR / rebase-PR" — it compares two
snapshots. The merge strategy does not affect the result of the **graph delta**:
only the states of the source tree at the two commits matter. This is an intentional
property: the same logic for all git hosts and merge strategies.

## Minimal CI snippet

See the full example: [example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).

Key points:

```yaml
- uses: actions/checkout@v4   # shallow (depth 1) — we don't pull history
- run: |
    base="${{ github.base_ref }}"
    [ -z "$base" ] && base="${{ github.event.merge_group.base_ref }}"
    base="${base#refs/heads/}"
    git fetch --no-tags --depth=1 origin "+refs/heads/$base:refs/remotes/origin/$base"
    ./archcheck --diff "origin/$base..HEAD" .
```

### Why a shallow base-fetch

`archcheck --diff origin/<base>..HEAD` performs only two git operations:
`git diff --name-only A B` (comparing **two trees**) and `git worktree add
--detach <ref>` (checking out **one tree**). No `rev-list`, no `merge-base`, no
traversal of the `A..B` range: `parseRevspec` simply splits the string on `..` into
two refs. So **only the base snapshot** is needed (one commit + tree), not history.

`actions/checkout@v4` does a shallow clone by default (`--depth=1`) — HEAD is there,
but `origin/<base>` is not. We fetch it as a **single** snapshot:
`git fetch --no-tags --depth=1 origin "+refs/heads/<base>:refs/remotes/origin/<base>"`.
Verified on a shallow sandbox: with 2 commits in the local graph, `merge-base`
unavailable, `archcheck --diff` builds a real baseline graph and returns a correct
verdict. On large repos this saves tens of seconds / hundreds of MB versus
`fetch-depth: 0`.

`fetch-depth: 0` (the entire history) remains a **simple but heavy** fallback if you
don't want a separate fetch step. The equivalent via explicit SHAs (e.g. for
merge strategies where base/head are known from the payload):

```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 1
- run: |
    git fetch --depth=1 origin ${{ github.event.pull_request.base.sha }}
    git fetch --depth=1 origin ${{ github.event.pull_request.head.sha }}
    archcheck --diff "${{ github.event.pull_request.base.sha }}..${{ github.event.pull_request.head.sha }}" .
```

`git worktree add --detach <sha>` works with any locally known object — shared
history between refs is not required (archcheck does not call `git merge-base`
itself).

### Merge queue (`merge_group`)

If the repo uses GitHub Merge Queue, the workflow must trigger **both** on
`pull_request` **and** on `merge_group` ([docs.github.com — Managing
a merge queue](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-a-merge-queue)).
The check-run name must match across both triggers, otherwise the queue will hang
waiting for a check that will never arrive:

```yaml
on:
  pull_request:
    branches: [main]
  merge_group:
```

In `merge_group`, `HEAD` is already the queue's generated merge commit; base is taken
from `github.event.merge_group.base_ref` (in this event `github.base_ref` is empty),
and the revspec `origin/<base>..HEAD` remains correct.

## Edge cases

| Scenario                                | Behavior                                               |
|-----------------------------------------|--------------------------------------------------------|
| Shallow clone, baseline ref does not resolve | **exit 2** — error on stderr, no report produced; never a silent empty-tree compare or phantom "everything added" gate. Fetch the base ([#144](../backlog/new/144_min_diff_unresolved_baseline_exit2.md)) |
| Force-push to the PR branch             | OK — we compare by revspec, the previous run is not needed |
| Merge commit as HEAD                    | OK — the worktree takes a snapshot tree, not a diff against parents. Test coverage — [#022](../backlog/completed/022_min_diff_merge_commit_coverage.md) |
| PR does not touch C/C++                 | fast-path: exit 0 and an empty report without building graphs. See [#023](../backlog/completed/023_maj_diff_skip_when_no_cpp_changes.md) |
| Rebase/squash on the PR side            | no difference — snapshots, not history                 |
| `compile_commands.json` missing         | for the shipped rules (SF.7/8/9, Lakos, drift) it is **not needed** — the include-scanner backend. Semantic rules in v0.2+ will require it in both worktrees |
| Submodules                              | `git worktree add` **inherits `.gitmodules` but does not initialize them**. If the architecture depends on code in a submodule — after `materializeRef` you need `git -C <worktree> submodule update --init --recursive`. archcheck does not do this now; documenting or automating it is an open question |
| Draft PR                                | GitHub sends `pull_request` events for drafts too; the tool runs as for a normal PR. Disable via `if: github.event.pull_request.draft == false` in the workflow |

## What CI is handed

`stdout` — a text report or JSON (`--format=json`). Text shows the gating block and
the advisory blocks, then a final line `gate: ok|fail`. JSON has a stable schema
`version: 1`, with top-level `gate`, `gating`, and `advisory`.
See [regression_report.h](../include/archcheck/diff/regression_report.h),
[regression_report.cpp](../src/diff/regression_report.cpp).

`exit code` — the gate for CI (see above).

`stderr` — diagnostics (git errors, not-a-repo, etc.).

SARIF — a future/v0.2+ channel (see [docs/architecture-spec.md](architecture-spec.md)
§"Roadmap"). JSON is already shipped for `check` and `--diff`.

## Channels for publishing into a PR

A CLI tool invoked from a CI step has three channels for "showing the result in the
pull request UI". They are **not interchangeable** — archcheck is designed for the
first two:

### 1. Check Runs / Step Summary (recommended by default)

A failed step in the workflow automatically draws a red Check next to the PR — this is
enough as a minimal gate. Beyond that, it is recommended to pipe archcheck output into
`$GITHUB_STEP_SUMMARY`, so the full report opens with a single click from the run page
without a separate artifact download.

When you need a named check with annotations (`::warning file=...,line=...`)
— see the format of
[GitHub workflow commands](https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions).
Anchoring an annotation to a line requires line info in `EdgeRef` — as long as the
reporter shows only paths, annotations will be file-level.

### 2. Sticky PR comment

For an architectural report (high-level, not line-by-line) a sticky comment is the
right channel. The de facto standard is
[marocchino/sticky-pull-request-comment](https://github.com/marocchino/sticky-pull-request-comment):
it uses `header:` as an identifier for update-in-place, does not generate noise on
force-push to the PR branch, and gets by with `GITHUB_TOKEN` without a PAT.

A close analog from the world of dependency tools is
[depcruise-pr-check](https://github.com/marketplace/actions/depcruise-pr-check),
which draws a mermaid graph of added dependencies in a PR comment.

A ready-made snippet (requires `permissions: pull-requests: write` at the job level):

```yaml
- name: Run archcheck --diff
  id: archcheck
  run: |
    ./archcheck --diff "origin/${{ github.base_ref }}..HEAD" . \
      | tee archcheck-diff.txt
  continue-on-error: true

- name: Post sticky PR comment
  if: always() && github.event_name == 'pull_request'
  uses: marocchino/sticky-pull-request-comment@v2
  with:
    header: archcheck-diff   # update-in-place key; prevents comment spam on force-push
    recreate: false
    path: archcheck-diff.txt

- name: Fail the job if archcheck found a regression
  if: steps.archcheck.outcome == 'failure'
  run: exit 1
```

`header: archcheck-diff` — the identifier for update-in-place: on a repeated push to
the PR branch, the action finds its previous comment and updates it rather than
creating a new one.

`if: always() && github.event_name == 'pull_request'` — two conditions:
`always()` publishes the report even on a regression (exit 1);
`github.event_name == 'pull_request'` skips the step for `merge_group`
(there is no PR to comment on there).

`continue-on-error: true` on the archcheck step + a final fail step — the standard
"let all steps run, but fail the job" pattern.

Full working example: [example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).

### 3. Review comments on specific lines

This is a tool for linters (clang-tidy, reviewdog `github-pr-review`).
For archcheck it is **fundamentally unsuitable** — an architectural regression often
has nothing to anchor line-by-line: a new cycle `A→B→C→A` can appear from a
merge-from-main, even though the PR itself touched none of those lines.

### 4. SARIF + GitHub Code Scanning (optional)

`github/codeql-action/upload-sarif@v3` accepts SARIF 2.1.0 (≤10 MB gzip,
≤25,000 results) and renders results in the "Security → Code scanning" tab.
An important limitation:
[free only for public repos](https://docs.github.com/en/code-security/code-scanning/troubleshooting-sarif-uploads/ghas-required);
for private it requires GitHub Advanced Security (a paid license).
Therefore SARIF for archcheck is an **optional** v0.2 channel, not the primary one.

### reviewdog taxonomy (for reference)

The [reviewdog README](https://github.com/reviewdog/reviewdog) splits CI reporters by
the same criterion:

| Reporter              | Where it writes           | Applicability to archcheck |
|-----------------------|---------------------------|--------------------------|
| `github-pr-check`     | Checks tab (file-level)   | yes — the primary mode   |
| `github-pr-review`    | review comments by line   | no — nothing to anchor   |
| `github-check`        | Checks tab outside a PR (push) | for the main branch |

### What archcheck does **not** do (and why)

- **Does not call the GitHub API.** The tool stays git-host-agnostic. Public/
  private cloud, GitLab self-hosted, Bitbucket, Gitea — the same everywhere.
  Publishing is the job of the CI step after archcheck.
- **Does not comment on the PR itself.** Publishing is an adapter step in YAML
  (channel 2); a ready-made snippet with `marocchino/sticky-pull-request-comment`
  is above and in
  [example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).
- **Does not parse `.diff` / `.patch`.** Graph snapshots give a precise answer;
  a line-by-line diff would not show "a new cycle A→B→C→A appeared" if the PR
  itself added none of those edges.

## Local pre-push run

The local equivalent of CI validation:

```bash
git fetch origin main
archcheck --diff origin/main..HEAD .
echo "exit=$?"
```

Or against the previous commit:

```bash
archcheck --diff HEAD~1 .          # baseline = HEAD~1, current = WORKTREE
```

If the worktree is "dirty" (uncommitted edits), they end up in `current`
automatically — this is convenient for checking "did I break the architecture with
what is currently in the editor".

## Related tasks

- [#018](../backlog/completed/018_crt_git_diff_analysis.md) — implementation of `--diff` (completed).
- [#022](../backlog/completed/022_min_diff_merge_commit_coverage.md) — test for a PR with a merge commit (completed).
- [#023](../backlog/completed/023_maj_diff_skip_when_no_cpp_changes.md) — fast-path when there are no C/C++ changes (completed).
- [#025](../backlog/completed/025_maj_pr_comment_integration.md) — publishing the report as a comment on the PR (completed).
