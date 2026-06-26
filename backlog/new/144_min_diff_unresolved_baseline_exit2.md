# [CLI] --diff: unresolvable baseline ref → exit 2 instead of silent empty tree

**Created:** 2026-06-25
**Status:** new
**Module:** CLI / git
**Priority:** minor
**Complexity:** S
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
