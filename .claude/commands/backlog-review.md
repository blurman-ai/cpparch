Review backlog: find stale tasks, estimate difficulty, highlight quick wins.

No arguments. Run as `/backlog-review`.

## Document contract (ownership)

| Document | What it holds | Tense | Update trigger |
|---|---|---|---|
| `backlog/new/` | queue of active tasks not yet started | present | `/create-task`, move from `wip/` (rollback) or `future/` (phase came up) |
| `backlog/wip/` | tasks in progress (have a `Start date:`) | present | `/issue`, `/checkpoint` |
| `backlog/completed/` | completed tasks + system documentation (DoD sections) | past, append-only | `/fix-issue` |
| `backlog/future/` | post-MVP tasks / explicitly deferred to v0.3+ | mutable | scope decision, phase downgrade |
| `backlog/pending/` | parking lot, **not a queue** | — | manually, not from skills |
| `backlog/backlog_review.md` | queue snapshot + classification | present | this skill (`/backlog-review`) |

**Files managed by this skill:** `backlog/*` (except `pending/` — don't touch without an explicit command).
**Files under `/status-review`:** `CHANGELOG.md`, `docs/ROADMAP.md`, `~/projects/archcheck-journal/milestones.md` (private companion repo, #167). No overlap.
**This skill does NOT edit** CHANGELOG / ROADMAP / milestones / spec / README / CLAUDE.md.

## Steps

1. **Collect all tasks**:
   ```
   Glob(pattern="*.md", path="backlog")
   Glob(pattern="*.md", path="backlog/completed")
   ```
   Read each one (you can use parallel `general-purpose` agents in groups of 5-10, model=sonnet).

2. **Classify the active ones** (in `backlog/`):
   - **Difficulty**: `quick_win` (< 1 day) / `medium` (1-3 days) / `hard` (> 3 days) / `unknown`.
   - **Module**: CONFIG / GRAPH / SCAN / RULES / REPORT / CLI / FIXTURES / BUILD / DOCS.
   - **Blockers / dependencies** — from the `Blocked by:` / `Blocks:` sections.
   - **Whether there's real analysis or it's an empty template.**

3. **Find stale tasks**:
   - Files not updated in > 30 days (by `git log`).
   - Files whose outcome is already done in another task (look for matches in `backlog/completed/`).
   - Templates with no analysis where nothing has moved.

   For each stale one, propose:
   - **Delete** — empty template, no analysis.
   - **Move to completed** — has valuable analysis, even if done differently.
   - **Keep in WIP** — a plan for later, justified.

4. **Find duplicates** — tasks overlapping in meaning. Propose to merge or note `Related:`.

5. **Summary report** — save to `backlog/backlog_review.md` with the date:

   ### Stale
   | File | Reason | Recommendation |
   |------|---------|--------------|

   ### Quick wins
   | File | Goal | Module |
   |------|------|--------|

   ### Medium tasks
   | File | Goal | Module | Difficulty |
   |------|------|--------|-----------|

   ### Hard / blocked
   | File | Blocker |
   |------|--------|

   ### Without analysis (needs research)
   | File | What's missing |
   |------|----------------|

   ### Duplicates / related
   | Files | Proposal |
   |-------|-------------|

6. **Optional — TASK_TRACKER**: if `backlog/TASK_TRACKER.md` exists — update statuses (`[ ]` / `[x]`), add new ones, remove deleted ones, recompute progress.

7. **Summary to the user**:
   - Total tasks.
   - How many are stale.
   - How many quick wins.
   - How many need analysis.
   - How many are blocked.

## What it does NOT do

- Doesn't edit `CHANGELOG.md` / `docs/ROADMAP.md` / `~/projects/archcheck-journal/milestones.md` (private companion repo, #167) — that's `/status-review`'s job.
- Doesn't edit `docs/architecture-spec.md` / `README.md` / `CLAUDE.md` / `AGENTS.md` — that's design / framing.
- Doesn't touch `backlog/pending/`.
- Doesn't move tasks between `new/` / `wip/` / `completed/` itself — only proposes. Moves are done by `/issue`, `/checkpoint`, `/fix-issue`.
- Doesn't commit. Only writes to `backlog/backlog_review.md` and shows a summary.
- Doesn't run build / tests / lizard.

## Tone

Dry, with tables. Group by module, not by date. Cross-check against `completed/` — don't miss what's already done. If a task is stale — say so plainly, don't soften it.

## Tips

- Parallel agents (`general-purpose`, `model=sonnet`) — for reading groups of files.
