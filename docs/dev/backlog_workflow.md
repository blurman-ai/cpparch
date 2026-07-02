# Backlog in AI-driven development

How to manage tasks when the primary executor is an LLM assistant (Claude Code or equivalent), and the human is the author / reviewer / final voice.

The document describes **two working modes**:

- **Mode A — without an external tracker.** Source of truth is the filesystem. ID `NNN` in the filename. State = folder. Used in this repo (cpparch).
- **Mode B — with an external tracker** (Jira / GitHub Issues / Bitbucket / Linear). Source of truth is external. Local files are a _depth-layer_ for each active task: context, plan, progress for the assistant's next session. ID is external (`PROJ-123`), `NNN` optional as a local anchor.

Most of the principles and rituals are shared. The differences are in the _source of truth_, state transitions, and ID format.

The lifecycle canon for mode A is [`../../backlog/README.md`](../../backlog/README.md).

## Why files (shared across both modes)

An AI assistant loses context between sessions. A local task file is an **external state carrier** that can be trusted more than its own memory. Even when Jira is available:

1. **Handing context to a new session.** A task file in `wip/` is the input for the next conversation: the assistant reads the header + "Done" + "Next steps" and continues. A single file fits in context. A Jira ticket doesn't read that way — there are comments, noise, statuses in the side panel.
2. **Documentation as a byproduct.** A closed task in `completed/` with the sections **How it works / What controls it / What it relates to / Diagnostics** is the best possible module documentation: it explains the _decision_. Jira comments usually don't become that — they hold a conversation, not a doc.
3. **Stable anchors.** `#NNN` or `PROJ-123` is a permanent ID. The commit `feat(rules): add SF.9 (PROJ-123)` still reads a year later.
4. **Protection against memory regressions.** An AI eagerly "finishes" what is already done. A search over `backlog/completed/` + `git log --grep="<id>"` is a cheap check independent of any external tracker.
5. **State is visible via `ls`.** What is going on right now is `backlog/wip/`. Jira filters diverge from reality more often than a folder does.

## When to use which mode

| Scenario | Mode |
|---|---|
| Solo + AI, feature-oriented greenfield development | **A** (no tracker) |
| Open OSS project, GitHub Issues for external reports | **B** (GitHub Issues) |
| Team, corporate process, Jira/Bitbucket | **B** |
| Stream of customer bugs, release batches | **B** + (optionally) a summary `TASK_TRACKER.md` per batch |

You can start with A and switch to B once an external tracker is introduced. The `NNN` ID in existing files is preserved and keeps working as a local anchor — in parallel with the external one.

## Shared principles

### One file — one task

Each task lives in its own `.md`. The file is the _entire_ context of the task: goal, context, plan, progress, decisions, changed files, fixtures. Getting bloated — split it, add `Related:`. Coming as a batch — each subtask gets its own ID.

### State = folder

```
backlog/
  new/        — created, not started
  wip/        — in progress (has a Start date)
  future/     — deferred to a later release
  completed/  — done + full documentation of the solution
  pending/    — parking lot (NOT a queue, don't touch without an explicit command)
```

A state transition = `git mv`. The _folder_ is the only local truth about state. Caught by `ls`, `find`, `Glob`. In mode B the folder mirrors the external tracker's status for active tasks (see below on synchronization).

`new/` vs `future/` is _timing_, not priority.

### Dates are mandatory

- **Creation date** — always. Catches what has gone stale in `new/`.
- **Start date** — when moving to `wip/`. Catches "stuck without movement".
- **Completion date** — when moving to `completed/`.

Without dates the assistant can't tell "recent" from "forgotten". Time is external to it.

### `completed/` is documentation

Before `git mv` into `completed/`, four sections are added:

- **How it works** — principle, algorithm, data flow.
- **What controls it** — CLI flags, env vars, config fields.
- **What it relates to** — modules, dependencies, which tasks come next.
- **Diagnostics** — logs, metrics, key debugging spots.

The goal: the code could be wiped and reconstructed from the contents of `completed/`. Example — [`008b_blk_include_scanner_naive_extraction.md`](../../backlog/completed/008b_blk_include_scanner_naive_extraction.md).

These sections are written **locally** in both modes. The external tracker doesn't claim this space — it's about status and discussion, not about how the solution is built.

### Links between tasks

```markdown
**Blocks:** #004 (project_skeleton)
**Blocked by:** PROJ-123 (auth_refactor)
**Related:** #008 (dependency_graph_foundation)
```

The ID is a stable anchor, the name in parentheses is for readability. In mode B you can mix local `#NNN` and external `PROJ-123`.

### Fixtures (for archcheck)

This is a tool project: "if a feature can't be tested with fixtures — we don't implement it" ([`docs/MVP.md`](../MVP.md)). Each rule → `fixtures/<rule>/pass/` + `fixtures/<rule>/fail_*/`. The `/create-task` template adds a fixtures checklist.

---

## Mode A — without an external tracker

Source of truth is the filesystem. Used in cpparch.

### ID

A 3-digit `NNN` in the filename: `NNN_<priority>_<slug>.md`. Assigned by `/create-task` (max existing + 1). **Never changes** — it's the anchor in commits, links, and the assistant's memory.

Priority changes → rename the file (`maj` → `crt`), the ID stays.

### Lifecycle

1. **Creation** — `/create-task <name>` → file in `backlog/new/NNN_<priority>_<slug>.md`, status `new`.
2. **Start** — `git mv backlog/new/NNN_*.md backlog/wip/`, set `**Start date:**`, status `wip`. A significant task → feature branch `<type>/<NNN>-<slug>` (see [`git_workflow.md`](git_workflow.md)). A small one → direct push to master.
3. **Progress** — `/checkpoint` updates the file (Done / Next steps / Changed files).
4. **Commits** — `/commit` uses Conventional Commits and `(#NNN)`.
5. **Completion** — `git mv backlog/wip/NNN_*.md backlog/completed/`, status `done`, add the DoD sections.
6. **Review** — `/backlog-review` and `/status-review` periodically reconcile the state.

### Strengths

- Zero dependency on external systems. Works offline, on a plane, in a repo with no network access.
- The ID and the file are always together — nothing to drift out of sync.
- A simple grep / git log over `#NNN` gives the full history of the task.

### Weaknesses

- Not visible to people who have no repo access.
- No native multi-assignee / SLA / escalations.
- With a stream of > 30 simultaneously open tasks you lose the overview.

---

## Mode B — with an external tracker

Source of truth is Jira / GitHub Issues / Bitbucket / Linear / etc. Local files are a _depth-layer_: what doesn't fit in the ticket and is needed by the assistant between sessions.

### What lives where

| Where | What |
|---|---|
| **External tracker** | official status, assignee, priority, SLA, discussion with customer/team, links to PRs |
| **Local file** | implementation plan, context from the code, progress between assistant sessions, **DoD sections at closing**, fixtures |
| **Commit** | link to the external ID (format — see below), AI trailers, local hash |

The principle: **external tracker — what and why; local file — how and where**. They don't duplicate each other, they complement.

### ID

The filename is `<EXTERNAL_KEY>_<slug>.md` or `<NNN>_<EXTERNAL_KEY>_<slug>.md`. Examples:

- Jira: `PROJ-123_auth_refactor.md`
- GitHub Issues: `gh-42_metric_regression.md` or just `042_metric_regression.md` (if `NNN == issue number`)
- Bitbucket (gm): `521_rmi_crash.md` (NNN = Bitbucket issue ID).

The local `NNN` is optional. If used — `NNN` is immutable (as in mode A); the external ID may come later (the ticket was created the next day) — then it's recorded in the header:

```markdown
**External ID:** PROJ-123
**Local ID:** #042
```

### Lifecycle

1. **Creating the ticket** in the external tracker (manually or via `/gh-issues` for GitHub). The task gets an external ID.
2. **Import into the local backlog** — [`/gh-issues`](../../.claude/commands/gh-issues.md) or equivalent: create a file in `backlog/new/` with a link to the ticket in the header. If the task is small and the plan is obvious — this step can be skipped, no local file at all.
3. **Starting work:**
   - External tracker: status → "In Progress", assignee → yourself.
   - Locally: `git mv` into `backlog/wip/`, `**Start date:**`.
4. **Progress** — `/checkpoint` updates the _local_ file. Only meaningful updates go to the external tracker (once a day / on reaching a milestone), not every step.
5. **Commits** — the link format depends on the tracker:
   - Jira **Smart Commits**: `feat(auth): refactor session store (PROJ-123)` — Jira automatically attaches the commit to the ticket.
   - GitHub Issues: `fix(rules): handle empty config (#42)` — `#42` is linked automatically.
   - Bitbucket: `fix(rmi): vnc reentrancy guard (#521)`.
   - Extra keywords: `Closes PROJ-123`, `Fixes #42` in the commit/PR body close the ticket on merge.
6. **PR** — a link to the ticket in the PR description; the PR closes the ticket automatically via the `Closes`/`Fixes` keyword.
7. **Completion:**
   - External tracker: status → "Done" / "Closed" (automatically after merge if there is a `Closes`).
   - Locally: `git mv` into `backlog/completed/`, `**Completion date:**`, **DoD sections**.
8. **Review** — `/status-review` reconciles git+files with the local documents. The external tracker is reviewed with the tracker's own tools (its own filters).

### Strengths

- Visibility for the team and stakeholders.
- Standard processes (SLA, escalations, reporting) for free.
- Smart Commits / GitHub linking — tying commits to tickets without manual work.

### Weaknesses

- Double bookkeeping if sloppy: status "Done" in the external one, the local file still in `wip/`. Cured by `/status-review`.
- Dependency on the network / external service.
- The temptation to "write everything in Jira" — after which no documentation of the solution remains in the repo.

### What _not_ to delegate to the tracker

- **DoD sections** (How it works / What controls it / What it relates to / Diagnostics) — only in the local file. The external tracker buries them in comments.
- **Implementation plan with notes for the assistant's next session** — only locally. The assistant won't re-read a Jira comment.
- **Changed files + hash** — only locally, in the header of the completed task. It's in Jira, but inaccessible to the assistant offline.

### Synchronization rules

- External status ↔ `backlog/` folder: `In Progress` ⇔ `wip/`, `Done`/`Closed` ⇔ `completed/`, `Backlog`/`To Do` ⇔ `new/` or `future/`. Reconciled by `/status-review` (or its extended version).
- Divergence → the truth is with whoever is fresher. Usually: the ticket was closed by a merge, the local file is still in `wip/` → move + DoD sections.
- The external tracker does _not_ dictate the filename priority. If a local `NNN` is used, the priority in the name (`blk`/`crt`/`maj`/`min`) lives by mode A's rules.

### Example: gm (Bitbucket Issues)

See [`~/projects/gm/docs/wip/521_rmi_crash.md`](~/projects/gm/docs/wip/521_rmi_crash.md) — `521` is the Bitbucket issue ID, the file is the depth-layer: timeline, stack traces, hypotheses, "next steps". The Bitbucket ticket holds only status + a short update.

---

## Rituals (slash-commands)

All live in [`.claude/commands/`](../../.claude/commands/). Applicable in both modes unless marked otherwise.

### [`/create-task <name>`](../../.claude/commands/create-task.md)

Computes `NNN`, checks for duplicates, looks for related ones, creates a file from the template in `backlog/new/`.

In mode B — after creation: create the ticket in the external tracker (or the other way around: create the ticket → import).

### [`/gh-issues`](../../.claude/commands/gh-issues.md) _(mode B, GitHub)_

Imports open GitHub issues into `backlog/new/` as ordinary tasks. The channel "external stream" → "local backlog", without double bookkeeping.

There is no Jira equivalent — it's done ad-hoc via MCP or manually.

### [`/checkpoint`](../../.claude/commands/checkpoint.md)

Updates the active file in `wip/`: "Done", "Next steps", "Changed files".

**Why:** a note to the assistant's next session, not a report to a human. Run it after a meaningful block of work, not after every trivial action.

### [`/commit`](../../.claude/commands/commit.md)

Conventional Commits, link to the ID, AI trailers (`AI-Assisted`, `Verified: build+tests`, `Risk`, `Co-Authored-By`). Shows the message _before_ creating it — the human confirms.

In mode B — use the tracker's link format (Smart Commits / `#N` / `Closes`).

### [`/fix-issue`](../../.claude/commands/fix-issue.md) / [`/issue`](../../.claude/commands/issue.md)

Forces the assistant to _first read the task file_ and reconcile "Done" with the git reality, rather than recall from memory.

### [`/backlog-review`](../../.claude/commands/backlog-review.md)

A report on what's stale in `new/`, hanging in `wip/`, quick wins, blocked. Ignores `pending/`.

In mode B — additionally reconcile against the tracker's active tickets: "in the tracker, not local" / "closed in the tracker, open locally".

### [`/status-review`](../../.claude/commands/status-review.md)

Walks over git+files and reconciles against `CHANGELOG.md`, `docs/ROADMAP.md`, `~/projects/archcheck-journal/milestones.md` (private companion repo, #167). Catches divergences of the documents from reality.

In mode B — extend it with a "local state ↔ external tracker" check (if MCP/API is available).

### [`/findings`](../../.claude/commands/findings.md)

At the end of a session: extract from the conversation what's worth putting into memory. Transfer of knowledge between assistant sessions.

---

## Anti-patterns

### General

- **Changing `NNN` on rename.** Never. The ID is an anchor, not metadata.
- **Putting everything in `wip/`** "because I'm thinking about it". `wip/` = has a `**Start date:**`, work is in progress.
- **`completed/` without DoD sections.** Without the sections the task isn't closed. Caught by `/status-review`.
- **Using `pending/` without an explicit command.** In this repo `pending/` is a parking lot, not a queue (memory `feedback_pending_folder.md`).
- **Creating a new task without searching for a duplicate.** `/create-task` searches itself — don't bypass it.
- **A commit without an ID** while working on a task. Then you can't link the fact to the cause via `git log --grep`.
- **"Closing with a checkbox".** A checkbox isn't enough — you need `git mv` + DoD + a commit mentioning the ID.
- **Scattering one task across several `wip/` files.** One file — one task.
- **Committing without an explicit command.** Finished the task — wait for `/commit` or "make a commit" (see CLAUDE.md).

### Specific to mode B

- **Writing DoD sections in ticket comments.** They'll get lost there under a layer of conversation. The solution stays in the repo.
- **Duplicating every `/checkpoint` into Jira.** The external tracker doesn't like noise — an update once a day / per milestone, not on every commit.
- **Closing locally without `Closes`/`Fixes` in the PR.** The ticket will stay hanging open — a divergence from the external tracker.
- **Trusting only the external ID, not creating a local file for a non-trivial task.** Then the assistant's next session starts from scratch.
- **Creating a local file for every little thing when the task is a one-line fix.** Extra noise. A local file is for tasks where there's _something_ to pass between sessions.

---

## Related documents

- [`backlog/README.md`](../../backlog/README.md) — the short canon of the lifecycle and format.
- [`docs/dev/git_workflow.md`](git_workflow.md) — GitHub Flow, Conventional Commits, SemVer, tags.
- [`docs/code_quality.md`](../code_quality.md) — thresholds on change size (≤ 50 lines, ≤ 2 files per commit).
- [`.claude/commands/`](../../.claude/commands/) — sources of all slash-commands.
