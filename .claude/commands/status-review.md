Walk through git + the filesystem and reconcile against the documents: `CHANGELOG.md`, `docs/ROADMAP.md`, `~/projects/archcheck-journal/milestones.md` (private companion repo, #167). Show the discrepancies and propose patches in a single batch.

No arguments. Run at the end of the day / after a series of tasks: `/status-review`.

The skill **only shows and proposes**. It changes nothing until an explicit "yes". Idempotent — a repeat run a minute later finds nothing new.

## Document contract (ownership)

| Document | What it holds | Tense | Update trigger |
|---|---|---|---|
| `CHANGELOG.md` | shipped per version | past, append-only | `feat:` / `fix:` / `perf:` → bullet in `[Unreleased]`; release cut → `[X.Y.Z] — YYYY-MM-DD` |
| `docs/ROADMAP.md` | scope per version + current focus + blockers | mutable | scope decision, defer/pull-forward, phase change, new/closed v0.X blocker |
| `~/projects/archcheck-journal/milestones.md` (private companion repo, #167) | dogfood-runlog | past, append-only | running the tool on someone else's code |

**Principle:** one fact — one place. Duplication between documents is a bug, caught by this skill.

**Files managed by this skill:** `CHANGELOG.md`, `docs/ROADMAP.md`, `~/projects/archcheck-journal/milestones.md` (private companion repo, #167).
**Files under `/backlog-review`:** `backlog/*`. No overlap.
**The "where we are now" snapshot** lives in the header of `docs/ROADMAP.md` (`## Current focus`). There's no separate STATUS file.

## Dive-marker — from what point we audit

The `[Unreleased]` section in `CHANGELOG.md` = "work since the last release".
The boundary = the last `## [X.Y.Z]` heading in CHANGELOG, or `git describe --tags --abbrev=0` if there are more tags than in CHANGELOG.

Everything that landed in `master` after this boundary — must be reflected either in `[Unreleased]`, or (if it's a scope decision) in ROADMAP.

## Steps

1. **Determine the dive-marker:**
   ```
   grep -nE '^## \[[0-9]+\.[0-9]+\.[0-9]+\]' CHANGELOG.md | head -1   # last release
   git describe --tags --abbrev=0 2>/dev/null                          # last tag
   ```
   Take `boundary` = the commit where the last `## [X.Y.Z]` appeared (or the tag vX.Y.Z itself). If there's neither — `boundary` = the first commit of the repo.

2. **Collect the delta:**
   ```
   git log <boundary>..HEAD --oneline
   git log <boundary>..HEAD --name-only --pretty=format:'%H %s'
   ```
   Save: the list of commits with types (`feat:` / `fix:` / `perf:` / `docs:` / …), the list of touched paths, the mentioned `#NNN`.

3. **Check A — CHANGELOG gaps.** For each commit from the delta with type `feat:` / `fix:` / `perf:`:
   - Is there a line in `[Unreleased]` of CHANGELOG.md describing the same thing? No — flag.
   - Don't propose rewriting `[X.Y.Z]` sections retroactively. Only an addition to `[Unreleased]`.

4. **Check B — ROADMAP gaps.** For each commit from the delta:
   - Does it contain a scope-decision marker (`(defer)`, moving a task to `backlog/future/`, `chore(tasks): close v0.X blocker`, an explicit commit fixing a blocker from `## Current focus`) — is it reflected in the corresponding version block of ROADMAP? No — flag.
   - A closed blocker from `## Current focus` must not remain in the list. An open blocker must be in the list.
   - Does the phase in the ROADMAP header (`v0.1 (close to release)` etc.) agree with reality? All blockers closed → the phase should be ready to change.
   - **Don't describe concrete shipped items in ROADMAP** — that's CHANGELOG's job. ROADMAP describes scope, not checkboxes.

5. **Check C — milestones gaps.** If the delta has commits with run markers (`dogfood`, new `Run N` in a commit body, mentions of external projects) — is there a corresponding entry in `~/projects/archcheck-journal/milestones.md` (private companion repo, #167) with a close date? No — flag.

6. **Check D — completed tasks without DoD sections.** For each file that landed in `backlog/completed/` in the delta:
   - **How it works**, **What controls it**, **What it relates to**, **Diagnostics** must be present (see `backlog/README.md` §"Task lifecycle", item 5).
   - At least one missing — flag "incomplete".

7. **Check E — `#NNN` tasks from commits actually sit in `backlog/completed/`.** A commit references `(#NNN)` — find `NNN_*` in `completed/`. Not there — flag "dangling reference".

8. **Always ignore:**
   - `backlog/pending/` — parking lot, not a queue (see memory).
   - Commits with type `docs:` for `backlog/`, `docs/ROADMAP.md`, `CHANGELOG.md`, `~/projects/archcheck-journal/milestones.md` (private companion repo, #167) — that's part of the process, not content.
   - `chore(tasks):` commits that move files — that's already accounted for.

## Report

Form a summary table and show it to the user. Template:

```
### Discrepancies

| # | Where | What's not reflected | What I propose |
|---|-----|-----------------|---------------|
| 1 | CHANGELOG.md | feat(rules/drift): DRIFT.1/2 (#009 #040) | Add a line in [Unreleased] / Added |
| 2 | ROADMAP.md | blocker #049 closed, but still in Current focus | Remove the bullet |
| 3 | milestones.md | run on duckdb today | Add entry "Run N — duckdb" |

### Completed without DoD sections

| # | File | What's missing |
|---|------|------------|

### Dangling task references

| # | Commit | Reference | Where's the task |
|---|--------|--------|------------|

### Update the ROADMAP.md header
- was: `phase: <old phase>`
- becomes: `phase: <current phase>` (if all v0.X blockers are closed)
```

If there are no discrepancies — a short output: `✅ everything is in sync. The delta from <boundary> is clean.` And that's it, change nothing.

## After "yes"

Apply the patches in a single batch, **without committing**:

1. Edit `CHANGELOG.md` — append surgically to `## [Unreleased]`. Each bullet — one sentence in the style of existing ones.
2. Edit `docs/ROADMAP.md` — update `## Current focus` (remove closed blockers, add new ones) + the version block if necessary (scope decision); update the header (`phase`, date).
3. Edit `~/projects/archcheck-journal/milestones.md` (companion repo; commit+push there) — add a minimal run entry per the template at the end of the file (the user will fill in details). **Write in plain human language** (see §"Language of descriptions"): what was checked, why, what came out — without jargon and abbreviations. Exact numbers/commit not at hand — leave as `<!-- TODO -->`, don't invent them.
4. Edit the tasks from "without DoD sections" — add section headings with the placeholder comment `<!-- TODO -->` (don't invent content).
5. **Committing — NOT from this skill.** The user decides: `/commit chore(process)` or a manual commit. In the report, remind: "changes in the worktree, not committed".

## What NOT to do

- Don't rewrite `[X.Y.Z]` CHANGELOG sections retroactively. Only an addition to `[Unreleased]`.
- Don't describe concrete shipped items in ROADMAP (checkboxes) — that's CHANGELOG.
- Don't move files in `backlog/`. That's the job of `/checkpoint` and `/fix-issue`.
- Don't edit `backlog/*` — that's `/backlog-review`.
- Don't touch `backlog/pending/`.
- Don't run build / tests / lizard. That's not part of the audit.
- Don't edit `docs/architecture-spec.md`, `README.md`, `CLAUDE.md`. That's design / framing, not status.
- Don't invent the content of DoD sections. Only the skeleton.

## Language of descriptions (for entries that people will see)

Text that ends up in documents or in the report must be understandable to **anyone, including managers** — a person who doesn't read code and doesn't know our abbreviations.

- **Plain language, not technical.** Explain *what was checked, why, and what came out of it*, not *how it works inside*.
- **No jargon or acronyms.** Not "DRIFT.4.CYCLE precision 92% on the corpus", but "the rule turned out to be correct in roughly 9 cases out of 10". If a technical term is unavoidable — spell it out in the same phrase.
- **Each milestones run entry — with a line "main takeaway in plain words".** One sentence a non-technical reader will understand.
- This applies to milestones entries, ROADMAP wording, and the report itself. Internal dry data (commit, numbers, task IDs) can be kept next to it as a clarification, but the gist — in human language.

## Report tone

Structurally — with tables, so the discrepancies are visible at once. But **the wording in the tables — in human language** (see §"Language of descriptions"), not telegraphic jargon. Without retelling what was done — the user remembers it anyway. The goal is to catch the **discrepancy between reality and the documents**, not to report on the work.
