# [CLI] --diff: unresolvable baseline ref → exit 2 instead of silent empty tree

**Created:** 2026-06-25
**Status:** wip — implemented + tested this session (uncommitted); awaiting /commit + move to completed/
**Module:** CLI / git
**Priority:** minor
**Complexity:** S

## Done (this session)

- `warnIfBaselineUnresolved` (warn-only) → `baselineResolves` in
  [src/cli/diff_command.cpp](../../src/cli/diff_command.cpp): an explicit baseline ref
  that does not `git rev-parse --verify` now prints a diagnostic and `runDiffFullPath`
  returns **exit 2** before any report — no silent empty-tree compare, no phantom gate.
  The `WORKTREE` sentinel is exempt (needs no resolution); the legitimate empty-side
  case only ever arrives via a resolvable ref (the git empty-tree object), so it is
  untouched.
- e2e test updated (`tests/integration/diff/diff_workflow_e2e_test.cpp`, "#144"): bad
  baseline ref → exit 2, diagnostic on stderr, no `gate: fail`.
- Docs reconciled: `docs/ci_integration.md` edge-cases table + `docs/ci_usage.md` ⚠️
  note now say exit 2 (the #143 "incorrect line" fix — this change makes the docs'
  aspirational claim true).
- Verified live: `archcheck --diff nosuchref..HEAD .` → exit 2. 580/580 tests, dogfood 0.
**Blocks:** —
**Blocked by:** —
**Related:** #143 (shallow base-fetch), src/git/git_state.cpp, src/cli/diff_command.cpp

## Goal / finding

Currently with `archcheck --diff <ref>..HEAD`, if `<ref>` **does not resolve** (typo,
not fetched in shallow CI), archcheck does NOT fail with an error — it prints a warning and
compares against an **empty tree**: `baseline_nodes: 0`, everything "added". On a real
tree this yields a **false gate**: verified — `grown_cycles: 9`, `gate: fail`,
**exit 1**. That is, a broken checkout fails someone else's build with phantom cycles.

The documentation (`docs/ci_integration.md`, edge-cases table) claims "shallow,
baseline unavailable → exit 2" — this does not match the actual behavior.

## To consider / do

- If the baseline ref is **given explicitly** and does not resolve → exit 2 (config/git error),
  as stated in the exit-code contract. Not a silent empty tree.
- Preserve the legitimate "empty/orphan baseline" case, if any (e.g.,
  comparison against the very first state). Possibly distinguish "ref is syntactically
  present, but git doesn't know it" from "intentionally empty side".
- Update the edge-cases table in `ci_integration.md` to match the actual (new) behavior.

## Acceptance

- [ ] `--diff nonexistentref..HEAD` → exit 2, diagnostics on stderr, no phantom gate.
- [ ] pass/fail fixture; integration test on the real binary.
- [ ] Docs consistent with behavior.

## Do not do

- Do not break `--baseline` / `--drift-baseline` (there baseline is a file, not a git ref).
