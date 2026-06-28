# [CI][DOCS] new-clone-gate demo repo — live CI surface for DRIFT.NEW_CLONE

**Created:** 2026-06-28
**Start date:** 2026-06-28
**Completed:** 2026-06-28
**Status:** completed (demo live on v0.1.5; only manual step left = screenshots for README)
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

**LIVE (2026-06-28):** the demo is published at **https://github.com/blurman-ai/archcheck-demo**
— a public merge-accumulate showcase. **PRs #1–#14**, all CI green, run against released **v0.1.5**:
N1–N5 silent (#1–5); P1–P5 fire EXACT/whole/renamed `DRIFT.NEW_CLONE` (#6–10); **S1–S4 fire
STRUCTURAL** — partial / Type-3 copies (inserted line, inserted block, one token changed, extra
trailing logic) (#11–14). P4/P5 demonstrate detection of copy-paste of code already duplicated
earlier in the accumulated history (the bug fixed below). **PR comments are markdown** (`--format=md`,
#157): a gate-summary header, the findings as a bullet list, structural diff folded into `<details>`.
Each finding shows **both spans** (`lines A-B — clone of <file>:C-D`, v0.1.4) and **both are
clickable** — the introduced anchor and the source line-range (`#La-Lb`, v0.1.5). Driver:
`experiments/clone_gate_demo_156/run_accumulate.py --mode live` (negatives-first order keeps
the labels clean under accumulation; the workflow diffs the PR event's explicit
`base.sha..head.sha` with `fetch-depth: 0`, not `origin/main..HEAD`, which breaks under a
moving main + PR merge-ref).

**Scanner bug found + fixed via this demo (shipped in v0.1.2):** the `DRIFT.NEW_CLONE`
parent-guard keyed clone pairs by content only, so a freshly pasted copy of code already
duplicated elsewhere was suppressed (real copy-paste missed). Fix = location-aware fragment
key (`src/scan/new_clone_drift.cpp`), pinned by a regression test, FP-gated on a 250-repo /
2029-commit before/after probe (+6% firings, same precision class). Commits 7559ea5 (fix) +
c960e6c (release). See JOURNEY 2026-06-28 entries.

In-repo reproducible demo assets built and verified locally (2026-06-28), under
`experiments/clone_gate_demo_156/`:

- **`build_demo_repo.py`** — materialises a standalone demo git repo by *importing* the 10
  scenarios from `experiments/clone_gate_validation_123/run_control_set.py` (no re-authoring
  of the mutations). Produces `main` (monit-4.2 + seed + demo workflow/README/NOTICE) + one
  branch per scenario (10 branches, one commit each → one PR each). `--verify` reproves
  fire/silent with the local debug binary.
- **`templates/archcheck-pr.yml`** — PR-trigger workflow = `example_archcheck_pr.yml` + an
  inline-annotation step mapping `--diff --format=json` advisory violations to GitHub
  `::warning file=,line=::` workflow commands (via `jq`). Three surfaces: inline annotations,
  sticky comment, step summary.
- **`templates/README.md` + `NOTICE`** — public demo docs (30-sec story, 10-PR table, GPL notice).
- **`RUNBOOK.md`** — the live-push handoff (repo identity → `gh repo create` → push → 10 PRs →
  screenshots → link from archcheck README).

**Verified:** `build_demo_repo.py … --verify` → **10/10** scenarios behave as expected
(5 fire `DRIFT.NEW_CLONE`, 5 silent); annotation mapping emits e.g.
`::warning file=control.c,line=590::DRIFT.NEW_CLONE — copy-paste introduced (EXACT): clone of seed/widgets.c:17-22`
with repo-relative paths (GitHub renders inline). Workflow YAML parses (8 steps).
`jq`/`actionlint` not installed locally → mapping verified in Python; both exist on `ubuntu-latest`.

## In progress

- (empty — in-repo assets complete; remaining work is the supervised live push)

## Next steps (the supervised live push — see RUNBOOK.md)

1. Decide repo identity: wait for the public `archcheck` repo, or a throwaway demo repo first.
   This also decides where the release binary the workflow downloads is hosted/readable.
2. Run `build_demo_repo.py /tmp/archcheck-demo --verify`, `gh repo create`, push main + 10
   branches, open the 10 PRs, eyeball the live surface, capture screenshots, link from README.

## Key decisions

| Decision | Reason |
|---------|---------|
| split demo out of #123 | #123's feature core is done + pinned by E2E; the demo is outward-facing and needs supervision (#123 itself proposed the split) |
| advisory, not gating, in the demo | matches the shipped behaviour; the exit-1 gate is a v0.2 decision after corpus precision (#103/#124) |
| ~~push-trigger over PR-trigger~~ → **PR-trigger** | superseded 2026-06-28: `DRIFT.NEW_CLONE` is exit-0 advisory, so push events give no sticky comment and no red check. Inline PR annotations are the gold-standard surface (reviewdog/Semgrep/CodeQL); PR-trigger enables all three channels |
| base = monit-4.2 | real ~81-file C codebase → credible "real PR" story; already has pre-existing clones for the N4 parent-guard negative; GPL retained + NOTICE |
| inline annotations via `::warning::` workflow commands (not SARIF) | archcheck's SARIF reporter is planned, not shipped; mapping `--format=json` to workflow commands gives inline annotations today, no cloud service |
| in-repo assets now, live push handed off | outward-facing + needs supervision + repo identity unsettled; assets are reproducible & verified, so the live step is mechanical |

## Changed files

| File | Change |
|------|-----------|
| `experiments/clone_gate_demo_156/build_demo_repo.py` | new — demo-repo generator (imports #123 scenarios) |
| `experiments/clone_gate_demo_156/templates/archcheck-pr.yml` | new — PR workflow + JSON→`::warning::` step |
| `experiments/clone_gate_demo_156/templates/README.md`, `NOTICE` | new — demo repo public docs |
| `experiments/clone_gate_demo_156/RUNBOOK.md` | new — live-push handoff |
| (demo repo, external) | created by the runbook at go-live |
| archcheck `README.md` / landing | screenshot + link to the live demo (at go-live) |

## Fixtures (if a rule)

- n/a — not a new rule; the rule's fixtures/E2E are closed under #123. This is a live-CI demo.
