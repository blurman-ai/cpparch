# [CI][DOCS] new-clone-gate demo repo — live CI surface for DRIFT.NEW_CLONE

**Created:** 2026-06-28
**Start date:** —
**Status:** new
**Module:** CI][DOCS
**Priority:** major
**Difficulty:** unknown
**Blocks:** —
**Blocked by:** —
**Related:** #123 (new-clone-gate feature — this is its split-out Stage 2), #124 (corpus precision/recall), #103 (thresholds), #150 (repo translated to EN — public-readiness)

## Goal

Stand up an outward-facing GitHub test repo with a push-triggered workflow that runs
`archcheck --diff` on the 10-commit control set (5 positive / 5 negative) and shows how
`DRIFT.NEW_CLONE` renders live on the CI surface (red check / sticky comment / step summary).

## Context

Split out of **#123 Stage 2**. The new-clone-gate *feature* is already shipped and validated:
the `DRIFT.NEW_CLONE` rule + parent-guard live in `diff_command.cpp`, the 10-commit control set
passed **10/10 locally** (2026-06-20), and the behaviour is pinned by a committed Catch2 E2E
(`tests/integration/diff/diff_workflow_e2e_test.cpp`, tag `[newclone]`) + unit tests. So #123's
core MVP is done — this task is **only** the live-on-real-CI demo/validation, which is the part
that needs an outward-facing repo and human supervision (you can't fully fixture "how the red check
looks in Actions").

**Why it matters:** doubles as the public-facing showcase — the 30-second "this PR added copy-paste
you didn't mean to" story. Per the 2026-06-28 reprioritization (ROADMAP), the public wedge is the
diff guardrail + advisories, so a credible live demo of an advisory firing on real CI is part of the
public story. **Not a hard release blocker**, but the launch is weaker without it.

The control set and ground truth already exist:
`experiments/clone_gate_validation_123/run_control_set.py` (the 5+5 scenarios, see #123 §Validation).

## Execution plan

- [ ] Create the public test repo (under the `archcheck` public name once that exists; otherwise a
      temporary repo) with a small realistic C++ base (e.g. the monit-4.2 base used in #123 Stage 1).
- [ ] Add a push-triggered workflow:
      `archcheck --diff "${{ github.event.before }}..${{ github.sha }}" .`
      (PR-trigger variant: reuse `.github/workflows/example_archcheck_pr.yml` as-is).
- [ ] Assemble the same 10 commits (5 positives must fire, 5 negatives must stay silent — the
      negatives hit the FP classes: move, below-threshold, parent-guard, formatting-only).
- [ ] Push them one per commit; read the reports in Actions.
- [ ] Decide the surface: red check / sticky comment / step-summary — pick what reads best for a
      newcomer and capture a screenshot for the README/landing.

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Decide repo identity: wait for the public `archcheck` repo, or a throwaway demo repo first.
2. Stand up base + workflow, push the 10 commits, eyeball the live surface.

## Key decisions

| Decision | Reason |
|---------|---------|
| split demo out of #123 | #123's feature core is done + pinned by E2E; the demo is outward-facing and needs supervision (#123 itself proposed the split) |
| advisory, not gating, in the demo | matches the shipped behaviour; the exit-1 gate is a v0.2 decision after corpus precision (#103/#124) |
| push-trigger over PR-trigger | one pushed commit = one CI run = one report, simplest to drive the 10-commit set |

## Changed files

| File | Change |
|------|-----------|
| (demo repo, external) | base + `.github/workflows/*` + 10 commits |
| `README.md` / landing | screenshot of the live surface (when public identity is settled) |

## Fixtures (if a rule)

- n/a — not a new rule; the rule's fixtures/E2E are closed under #123. This is a live-CI demo.
