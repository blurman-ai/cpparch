# [CI][DOCS] --diff CI: shallow base-fetch instead of fetch-depth: 0

**Created:** 2026-06-25
**Status:** wip — the core (workflow + both docs' shallow-fetch snippets) was already shipped before this session; only the unresolved-baseline prose remained, reconciled this session via #144
**Module:** CI / DOCS
**Priority:** major
**Complexity:** S
**Blocks:** adoption on large repositories (slow checkout = first bad impression)
**Blocked by:** —
**Related:** #142 (release binary), #144 (unresolved-baseline footgun), docs/ci_usage.md, docs/ci_integration.md

## State at review (this session)

- `.github/workflows/example_archcheck_pr.yml` — **already** has no `fetch-depth: 0` and a
  depth=1 base-ref refetch step (`github.base_ref` + `merge_group.base_ref`). Done.
- `docs/ci_usage.md` Scenario 2 + `docs/ci_integration.md` "Why a shallow base-fetch" —
  **already** carry the shallow snippet as the default, with `fetch-depth: 0` as the heavy
  fallback. Done.
- **Remaining (done this session):** the prose claiming "unresolved baseline → warning +
  empty-tree compare → false gate possible" was the one incorrect line called out in this
  task. #144 changed the behaviour to exit 2, so both docs (ci_usage ⚠️ note + ci_integration
  edge-cases table) were updated to say exit 2. The "incorrect line" fix is now correct, not
  just patched.
- Net: this task is effectively complete; verify the acceptance checkboxes below and close
  alongside #144.

## Goal

Remove `fetch-depth: 0` (full git history) from the recommended `--diff` CI snippet.
On large repositories the full history is tens of seconds / hundreds of MB out of
nowhere, even though `archcheck --diff` needs **one slice of the base tree**, not the history.

## Context / finding

`archcheck --diff origin/<base>..HEAD` performs only two git operations
([src/git/git_state.cpp](../../src/git/git_state.cpp)):
- `git diff --name-only A B` — comparison of **two trees**, not a range of commits;
- `git worktree add --detach <tmp> <ref>` — checkout of **one tree**.

No `rev-list`, no `merge-base`, no traversal of the `A..B` range. `parseRevspec` just
splits the string on `..`. So **only the base snapshot** is needed (one commit + tree),
and `fetch-depth: 0` is a dumb over-fetch.

Verified locally (shallow clone depth=1 + `git fetch --depth=1 origin <base>`):
the graph has 2 commits (not the whole history), `merge-base` is unavailable, and
`archcheck --diff origin/<base>..HEAD .` builds a real baseline graph
(`baseline_nodes: 284`), `gate: ok`, exit 0.

## What to do

- `.github/workflows/example_archcheck_pr.yml`: remove `fetch-depth: 0`, add
  a depth=1 base-ref refetch step (`git fetch --no-tags --depth=1 origin
  "+refs/heads/$BASE:refs/remotes/origin/$BASE"`), keep the revspec
  `origin/<base>..HEAD`. Support base from `github.base_ref` and
  `github.event.merge_group.base_ref`.
- `docs/ci_usage.md` (Scenario 2): the same snippet; explain why the history isn't needed.
- `docs/ci_integration.md`: make shallow base-fetch the **default** (not "for
  very large monorepos"); keep `fetch-depth: 0` as the "simplest but
  heavy" fallback. Fix the incorrect line about "shallow without baseline → exit 2"
  (in fact — warning + empty tree + possible false gate, see #144).

## Acceptance

- [x] The recommended `--diff` snippet does not contain `fetch-depth: 0`.
- [x] There's a depth=1 base refetch step; the revspec resolves in CI.
- [x] The docs explain: a base snapshot is needed, not the history; merge-base is not required.
- [x] YAML is valid; verified on a shallow sandbox (cpparch and/or leadline).
- [x] The incorrect "unresolved baseline" line fixed — both docs now say exit 2 (made true by #144).

## Do not do

- Don't change C++ (the `--diff` behavior is already correct). Hardening the non-resolve is #144.
