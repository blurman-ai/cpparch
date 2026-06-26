# [DOCS] EXAMPLES_50.md: regenerate section A from LATERAL events

**Date created:** 2026-06-12
**Date started:** 2026-06-12
**Status:** done
**Module:** DOCS
**Priority:** minor
**Difficulty:** small
**Assignee:** Haiku
**Blocks:** —
**Blocked by:** —
**Related:** #111 (lateral_drift_fix_and_corpus_run — data source)

## Goal

Replace section A in `experiments/ai_repo_run/EXAMPLES_50.md` (25 lines of raw
graph-drift examples, among which is noise of the "single edge in Log.hpp" kind) with
25 lines of LATERAL events from `experiments/ai_repo_run/lateral_drift_new.csv`.

## Context

#111 validated the lateral drift criterion: 98 events, TP ≈ 87%
(see [docs/research/lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md)).
The old section A was assembled BEFORE the criterion — from raw `added_edges`, 95% of which
are activity, not drift. Section B (copypaste) — DO NOT touch.

All files verified live 2026-06-12:

- `experiments/ai_repo_run/EXAMPLES_50.md` — section A: table rows 1–25,
  heading `## A. Per-commit graph-drift — new dependency introduced by a commit`;
  section B starts with `## B. Copypaste — clone pairs (HEAD snapshot)`.
- `experiments/ai_repo_run/lateral_drift_new.csv` — 98 rows + header.
  Columns: `repo,sha,date,author_kind,repo_kind,grade,moduleA,moduleB,FID_B,level_A,level_B,I_A,I_B,example_edge`.
- Local clones of the repos: `~/oss/<owner>_<name>` (name =
  `repo` from the CSV with the FIRST `/` replaced by `_`). Not all repos in the CSV are on disk —
  check `os.path.isdir` per row.

## Execution plan

- [ ] Select 25 events from the CSV: round-robin over repos (no more than one event from
      a single repo while there are uncovered repos), within a repo prioritize grades
      CYCLE > SDP > NEW. This is the same selection algorithm as in the #111 eyeball.
- [ ] Generate the table rows in the format of the existing section A:
      `| # | repo | commit | signal | new edge (src → dst) |`, where
      - `repo` — local name (`Bitcoin-ABC_bitcoin-abc`);
      - `commit` — `` `sha` `` + subject (take
        `git -C ~/oss/<name> -c gc.auto=0 log -1 --format=%s <sha>`,
        truncate to 60 chars);
      - `signal` — grade + modules: `⟲LATERAL.CYCLE moduleA→moduleB`
        (for SDP `↧LATERAL.SDP`, for NEW `→LATERAL.NEW`);
      - `new edge` — `example_edge` from the CSV; format the src part as a clickable
        link `[path](../../oss/<name>/<path>)` ONLY if the file exists
        on disk; otherwise both parts as flat code in backticks.
- [ ] Replace in `EXAMPLES_50.md` the section A table rows (between the table
      heading and `## B.`) with the new 25. Rewrite the section A intro paragraph:
      "Each row: a LATERAL event of the lateral drift criterion (#111) — the first
      link between peer modules. Grades: ⟲CYCLE (closed a module cycle), ↧SDP
      (dependency on a less stable one), →NEW (first lateral link)."
- [ ] Self-check against the control-numbers table (below).

## Control numbers (contract)

| Check | Expected |
|---|---|
| Rows in the section A table after the edit | exactly 25 |
| Of them LATERAL.CYCLE | ≥ 14 |
| Distinct repos | ≥ 15 |
| Events from repo scylladb/scylladb | ≤ 2 |
| Section B | byte-for-byte unchanged |
| Clickable links whose file is not on disk | 0 |

## Don't do

- DON'T touch section B and the document header (except the section A intro paragraph).
- DON'T change `lateral_drift_new.csv` and the scripts `lateral_drift_scan.py` /
  `make_window_baselines.py`.
- DON'T commit: `experiments/` is in `.gitignore`, the file is local. Report and wait.
- DON'T recompute events or filter them by your own judgment — selection is
  strictly by the algorithm from the plan.

## Definition of done

All six control numbers match; the output
`grep -c '^| [0-9]' <section A>` = 25 is shown in the report.

## Done

- Section A in `EXAMPLES_50.md` regenerated: 25 LATERAL events from
  `lateral_drift_new.csv` (98 events), round-robin selection over repos,
  grade priority CYCLE > SDP > NEW — the same algorithm as in the #111 eyeball.
- Section A intro paragraph rewritten for the lateral drift criterion (glossary
  of grades ⟲CYCLE / ↧SDP / →NEW).
- Clickable links to the src file only if present on disk (22/25 rows);
  3 rows — flat code (files absent at those SHAs / the repo was still being cloned).
- Section B (copypaste) — byte-for-byte unchanged.
- Control numbers (contract) — all 6 match:

| Check | Expected | Actual |
|---|---|---|
| Rows in the section A table | 25 | 25 ✓ |
| LATERAL.CYCLE | ≥ 14 | 18 ✓ |
| Distinct repos | ≥ 15 | 19 ✓ |
| scylladb/scylladb | ≤ 2 | 2 ✓ |
| Section B unchanged | yes | yes ✓ |
| Broken links | 0 | 0 ✓ |

## In progress

- (empty)

## Next steps

- (empty)

## Key decisions

| Decision | Reason |
|---------|---------|
| Round-robin over repos, CYCLE-first | The same selection as in the #111 eyeball — comparability |
| Links only to existing files | 297 corpus repos deleted from disk (#111 §1) |

## Changed files

| File | Change |
|------|-----------|
| `experiments/ai_repo_run/EXAMPLES_50.md` | Section A: 25 new rows + intro paragraph (local, gitignored — no commit) |

## How it works

The selection is deterministic: events are grouped by repo, within a repo sorted
by grade priority (CYCLE > SDP > NEW), then round-robin — each pass takes
one top event from each repo until 25 are gathered. This guarantees coverage
of ≥15 repos and saturation of CYCLE before any repo gives a second event.

## Date completed

2026-06-12

## Note on the commit

The result (`EXAMPLES_50.md`) lives in `experiments/`, which is in `.gitignore` —
there's no commit with the artifact by design. Only the task file goes into the repository.
