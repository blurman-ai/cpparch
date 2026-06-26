# [BACKLOG] Separate live / historical / cancelled in the backlog (duplication/experiments tails)

**Created:** 2026-06-05
**Started:** 2026-06-05
**Completed:** 2026-06-05
**Status:** completed
**Module:** BACKLOG
**Priority:** minor
**Complexity:** M
**Blocks:** —
**Blocked by:** —
**Related:** #082 (alignment umbrella — parent, Slice 4 + problem #6)

## Goal

Make the backlog a reliable state system again: so that historical/cancelled
branches (especially around duplication spikes and `experiments/*`) don't look like
an active plan.

## Context (where the task comes from)

#082 (problem #6) established: some WIP/new tasks describe a cancelled/historical
duplication branch as if it were a live plan. In Slice 4, #082 fixed **only broken
links** and de-linked the deleted `experiments/line_duplication/*` in historical notes —
but **deliberately did NOT touch the meaning of tasks** ("Don't rewrite every backlog
file in a row. Don't change the meaning of old tasks if the problem is only links and statuses").

## What's needed

1. Run `/backlog-review`.
2. For each stale/duplication-history task in `wip/`/`new/` decide:
   - **alive** → leave as is;
   - **cancelled** → move to `completed/` with a cancelled/obsolete mark + a brief note why;
   - **historical** (matters as context) → a brief historical note, remove from the active queue.
3. Candidates for review (from the #082 findings): tasks about `experiments/*` spikes (#053, #054,
   #066, #072, #079) and duplication branches where an architectural decision has already cancelled the plan.

## Acceptance criteria

- [ ] In `wip/`/`new/` there are no tasks presenting a cancelled/historical branch as an active plan.
- [ ] The historical is separated from the current by an explicit mark, not deleted blindly (history preserved).
- [ ] After the pass, `/backlog-review` reads as an up-to-date state system.

## Notes

- Do NOT touch `backlog/pending/` without an explicit command (project rule).
- This is editorial work on the backlog, not edits to product code.

## Survey (2026-06-05, at start)

There are currently **12 tasks** in `wip/` — that's a smell in itself (too many "in progress").
The duplication/research cluster that needs adjudication:

| Task | About | Suspicion |
|---|---|---|
| #041 | audit hardcoded strings | non-duplication, check separately |
| #053 | line-based dup pass (spike port) | spike directory deleted; **but** dup is already in `src/` → possibly done, not cancelled |
| #054 | AI-repo duplication run (research) | research run; artifacts in untracked `experiments/` |
| #056 | partial dup pass (Type-3) | overlap with #072 (#056 port) — one may overlap the other |
| #060 | checker validation hardening | research validation cycle |
| #061 | #056 --diff rename-detection | depends on the status of #056 |
| #066 | AI-repo remeasure CLONEFAIL | research re-fetch, `experiments/` binding |
| #070 | FP fix proposals | partially implemented (P0/P1 in `src/`); P1.3 is now in #083 |
| #072 | full port of #056 into archcheck | overlap with #056 — which of them is alive? |
| #079 | corpus run graph+dup+AI | research run, `experiments/` binding |

### Why it wasn't done autonomously (honestly)

Adjudicating statuses is the **state of your plan**, not an objective fact from the repo.
For example, whether #056 and #072 are both alive (port and full port) or one cancels the other —
only the author knows. After #082 (duplication = advisory, not the center of the product) some research
runs (054/066/079) may have lost priority, but that's a product decision, not mechanics.
Flipping 9 statuses on a guess = misrepresenting the intent (exactly what the task forbids:
"Don't change the meaning of old tasks").

### What's needed from the user (to finish)

Go through `/backlog-review` **together** and for each task in the cluster say: alive / cancelled
(superseded by what) / historical. After that the mechanical part (moving to `completed/`,
historical-note, status update) is done quickly and safely.

## Adjudication verdict (2026-06-05) — the cluster is ALIVE, nothing to normalize

The user delegated the decision ("look through it yourself, your call"). Went through each task in the
cluster (titles, "Done", open `[ ]`, binding to code in `src/`):

| Task | Verdict | Evidence |
|---|---|---|
| #053 line-dup spike | **alive** (research closed, minimal mechanism remains) | "✅ Done (spike — do NOT reopen)" + "⬜ Remaining" |
| #054 AI-repo dup run | **alive research** | "Wait for the ×3 run to finish"; corpus cloned; open `[ ]` |
| #056 partial/Type-3 | **alive, almost ready** | Type-3 in `clone_classifier` (shipped); has "Next steps" |
| #060 validation hardening | **alive** | "[ ] Final report"; confirm layer = next step |
| #061 rename-detection | **alive** | bound to #060/#056 |
| #066 remeasure CLONEFAIL | **alive research (paused)** | resumable scripts, corpus 29GB/367 dirs, open harvest `[ ]` |
| #070 FP fix proposals | **alive hub** | guards implemented (P0+P1 in `src/`), but open proposal items; P1.3 part → #083 |
| #072 full port of #056 | **alive, NOT done** | detector in `src/` ✓, tests ✓, **but fixtures `fixtures/duplication/` do NOT exist** (hard requirement) + JSON-reporter pairs in question |
| #079 corpus run | **alive research** | "✅ Fully completed" (part) + background run "we look together" |

### Conclusion

The #082-Slice4 premise ("cancelled/historical branches hang as an active plan")
did **not** hold up on inspection. The cluster is **tidied-up but alive** research +
almost-ready ports, NOT zombie cancelled tasks. **None is subject to cancellation or
moving to `completed/`** by actual state: all of them have either real remaining
work, or an active/paused research run.

A mass "normalization" (cancel/archive) here would be a **mistake** — it would destroy
tracking of live work. The only real smell is the **sprawl** (12 tasks in `wip/`
at once), but that reflects real parallel research activity, not staleness.

The historical part (broken links, deleted `experiments/line_duplication`) is already
closed in #082-Slice4. Nothing more to do here.

**Closed:** adjudication done, verdict — nothing to normalize, the cluster is alive.
