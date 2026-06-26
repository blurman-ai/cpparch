# backlog/

archcheck task directory. One file = one task.

## Structure

```
backlog/
  new/*.md              — created, not yet started, candidates for the current release (v0.1 / v0.2)
  wip/*.md              — in progress (has a Started date)
  future/*.md           — created but explicitly deferred to a later release (v0.3+) — design / contract tasks
  completed/*.md        — finished (knowledge archive)
  dropped/*.md          — won't do: superseded / cancelled / obsolete; exact reason in the file's "Outcome" section
  backlog_review.md     — latest /backlog-review report (generated)
  TASK_TRACKER.md       — canonical tracker for the current release / MVP
  DEBT.md               — registry of gaps / tech debt; cleared during /backlog-review
```

## File-name format

```
NNN_<priority>_<short_name>.md
```

- **NNN** — 3-digit sequential ID, never changes. The `#NNN` reference lives forever.
- **priority** — 3-letter code:
  - `blk` — blocker (cannot move without it)
  - `crt` — critical (important now)
  - `maj` — major (significant, but can wait)
  - `min` — minor (nice to have)
- **short_name** — snake_case, readable task name.

Examples: `004_blk_project_skeleton.md`, `006_maj_spec_refactor.md`.

Priority changes → the file is renamed (`maj` → `crt`). The ID stays the same, `#004` references stay alive.

## Task lifecycle

1. **Create** — `/create-task <snake_case_name>`. File — `backlog/new/NNN_<priority>_<name>.md`, status `new`.
2. **Start work** — move to `backlog/wip/`, set `**Started:** YYYY-MM-DD`, status → `wip`. For significant tasks — create a feature branch `<type>/<NNN>-<slug>` (see [`docs/dev/git_workflow.md`](../docs/dev/git_workflow.md)). For small ones — direct push to master.
3. **Progress** — `/checkpoint` updates the file.
4. **Commits** — `/commit` uses Conventional Commits and references the task via `(#NNN)`. The commit hash goes in **Changed files**.
5. **Completion** — move to `backlog/completed/`, status → `done`, fill in the **How it works / What controls it / What it relates to / Diagnostics** sections. If the task changed product capabilities or fixed a bug — update `CHANGELOG.md` (bullet in `[Unreleased]`) and, if needed, `docs/ROADMAP.md` (`## Current focus`); manually or via `/status-review` at the end of the day.
6. **Cancellation (won't do)** — move to `backlog/dropped/`, record the exact reason and a link to the decision in the "Outcome" section: `superseded by #NNN` (displaced by another solution), `cancelled` (changed our mind), `obsolete` (lost its meaning). Knowledge that outlives the task (ported code, empirical findings) should be listed there too — dropped does not mean "erased from history". Difference from `completed/`: completed = goal achieved; dropped = goal cancelled by decision.
7. **Review** — `/backlog-review` periodically catches stale items (`new` with no movement, `wip` hanging for a long time). `/status-review` reconciles the state of the documents (CHANGELOG / ROADMAP / milestones) with the git reality.

## Why split new / wip / future

- **`new`** — the queue for the current release. We pull from here. "Not started" is a normal state.
- **`wip`** — something is actually being worked on *or* it has been abandoned. If a task hangs for a long time relative to its `Started` date — a clear "stuck" signal.
- **`future`** — design / contract tasks explicitly waiting for a later release (v0.3+). So they don't clutter the `new/` queue and don't add noise to `/backlog-review`. Such a task has `**Target release:** v0.X+` in its header. When the phase approaches — it moves into `new/`.

`new/` and `future/` differ by timing, not by priority: a `blk` task can live in `future/` — it's a blocker, but not for the nearest release.

## Active-task template

See `/create-task` — it creates a file with a ready scaffold.

## References between tasks

In the `**Blocks:**`, `**Blocked by:**`, `**Related:**` fields, reference by ID:
```
**Blocks:** #004 (project_skeleton), #002 (github_actions_ci)
```

The name in parentheses is for readability, the ID is the stable part. If a task moved to `wip/` or `completed/` — the ID doesn't change, nothing needs fixing.

## Principles

- **One file — one task.** If a task balloons — split it, set `Related:`.
- **A task without fixtures does not exist** (if it's a rule). The fixtures checklist is in the template.
- **Completed tasks are system documentation**, not a dump. The "How it works" section should let you understand the solution without reading the code.
- **Created date is mandatory.** It's the only way to catch stale items.
- **Started date is mandatory when moving to wip/.** It's the way to catch "hanging with no movement".
- **Release priority lives in `TASK_TRACKER.md`.** The file priority (`blk/crt/maj/min`) is not replaced, but `TASK_TRACKER.md` answers a separate question: what blocks specifically the nearest MVP / release.
