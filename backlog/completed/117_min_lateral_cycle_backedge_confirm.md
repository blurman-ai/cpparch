# [SCAN][GRAPH] Lateral CYCLE grader: confirm back-edge B→A on the prior state

**Created:** 2026-06-12
**Started:** 2026-06-12
**Status:** done
**Module:** SCAN][GRAPH
**Priority:** minor
**Difficulty:** small
**Executor:** —
**Blocks:** — (soft: a future DRIFT.4 rule)
**Blocked by:** —
**Related:** #115 (finding §8.3 of the corpus-run report), #111 (criterion)

## Goal

The `LATERAL.CYCLE` grade should be issued only when a back-edge B→A actually exists
on the graph state *before* the event. Right now the prototype marks CYCLE without
confirming it → a leaf target (`engine/assets`, `include/common`) falsely yields a cycle.

## Context

Eyeball #115 (top-20 from new repos, verification on the event commit) gave TP 85%,
CYCLE precision 84.6% (11/13). Both CYCLE FPs are one class:

- **AndreasNilsson123/Astraeus**: `engine/renderer→engine/assets` marked CYCLE,
  but no file in `engine/assets` includes `engine/renderer` either at `base~1`
  or at the commit. `assets` is a leaf, there is no cycle.
- **Tsoympet/PantheonChain**: `layer1→include/common/monetary/units.h` marked
  CYCLE, but `include/` holds only leaf common headers; there is no back-edge `include→layer1`.
  (Additionally: the `layer1` module was later renamed to `layer1-talanton`,
  so a HEAD grep is unreliable — check only at the event SHA.)

By definition of the criterion, CYCLE = a new edge A→B that closes a module cycle, i.e.
module B already had an edge into module A. Since the grader issues CYCLE without this — a precision bug.

Hypotheses about the cause (which one to check):
1. **Phantom edge** from a lost `removed` list (known ≤5.6%, see report §5.1):
   B→A was removed earlier, forward-only replay didn't drop it → a false back-edge.
2. **Module misattribution**: a file assigned to the wrong module (gluing parallel
   trees `include/X`+`src/X`?), creating a spurious B→A.

## Plan

- [ ] In `lateral_drift_scan.py` find the CYCLE grading site (where
      CYCLE vs SDP vs NEW is decided — near `mod_pair_first` / `mod_edges`).
- [ ] Add an explicit check: on the graph state BEFORE the event there exists at least
      one file edge from module B into module A. None → the grade is downgraded
      (CYCLE → SDP/NEW by the usual rules).
- [ ] If the cause is phantom edges: consider whether, when adding an
      edge to `mod_edges`, we need to store multiplicity (a refcount of file edges), so that
      "a module edge disappears when the last file edge is gone". But `removed`
      is lost → it can't be done fully; then a back-edge check by the actual presence of a
      file edge at the event moment is a sufficient conservative fix.
- [ ] Re-run on the two known-FP repos (Astraeus, PantheonChain) — both should
      stop producing CYCLE. Re-run the corpus, verify that the `exists` subset
      (183 repos) didn't lose CYCLE events (determinism #115 §8.2).

## Don't do

- Do NOT touch archcheck product code — this is a prototype in `experiments/` (gitignored).
  The lesson carries into the DRIFT.4 spec when the rule is implemented.
- Do NOT weaken detection for these two cases: the fix is fact confirmation, not a heuristic.

## Definition of done

- Astraeus and PantheonChain no longer yield CYCLE (downgrade or filter-out).
- The `exists` subset (183 repos) keeps its 43 CYCLE (or it is explained which
  of them were the same FP class and why they went away).
- The finding is reflected in `docs/research/lateral_module_drift_corpus_run.md` §8.3/8.5.

## Done

- **`confirm_backedge()`** in `lateral_drift_scan.py`: for a CYCLE candidate with a
  pre-existing back-edge (the `rev_pair in graph.mod_edges` branch) it confirms at
  `<sha>~1` that some file of module B actually `#include`s a file of module A.
  Include resolution as in archcheck (three paths):
  - relative `../x.h`/`./x.h` → `os.path.normpath` from the including file's
    directory, exact-match against normalized module-A paths;
  - root-relative `engine/x.h` → suffix-match (`endswith('/'+inc)`);
  - bare `x.h` → basename-match.
  Same-commit bidirectional cycles (`rev_pair in commit_pairs`) don't require
  confirmation. `None` (repo/sha unavailable) → keep the replay verdict.
- **Corpus effect:** CYCLE 153 → **146**, downgraded exactly **7 phantom
  back-edges** (Astraeus, 3× Aztec, GBAStation, ThemisDB, UE5), 0 lost
  events (495 total), 0 unexpected upgrades.
- **Each of the 7 verified independently** (git-grep over the `<sha>~1` tree, separate from
  confirm) — the back-edge B→A is absent in all of them.
- **Two recall traps along the way, both closed:**
  1. the first version required the module segment in the include text → it lost OpenADS/
     DIYComfort (root-relative includes without a prefix) → suffix resolution;
  2. the suffix didn't catch `../` includes → it lost 15 REAL cycles (NeoCalculator,
     Avalon, GameRewritten, Lightpad×6, abra×2) → `../` normalization added.
- **Side conclusion:** PantheonChain (FP #2 from eyeball #115) — actually a real
  *same-commit* cycle (`include↔layer1`, both edges in one commit); the eyeball
  missed the counter-edge. The eyeball's true CYCLE precision — 12/13 ≈ 92%.
- Report updated: `docs/research/lateral_module_drift_corpus_run.md` §8.3/§8.6.

## In progress

- (empty)

## Next steps

- (empty)

## Key decisions

| Decision | Reason |
|----------|--------|
| Confirm the edge fact, don't fix removed | removed is lost irretrievably; checking presence at the event moment is conservative and sufficient |
| Fix in the prototype, not the product | DRIFT.4 isn't implemented yet; the lesson goes into the spec |

## Changed files

| File | Change |
|------|--------|
| `experiments/ai_repo_run/lateral_drift_scan.py` | `confirm_backedge()` + integration into the grader (gitignored) |
| `experiments/ai_repo_run/lateral_drift_new.csv` | re-run, CYCLE 146 (gitignored) |
| `docs/research/lateral_module_drift_corpus_run.md` | §8.3 (fix + lesson), §8.6 (closed) |

## How it works

CYCLE grade = a new edge A→B given an existing back-edge B→A. The replay graph
(`mod_edges`) is forward-only and holds phantoms (removed-lists lost + archcheck's bare-name
resolution). So before grading CYCLE on a pre-existing back-edge, the scan goes
into git for the commit's parent and confirms the edge via real `#include`s of module B,
resolving paths three ways (relative/suffix/bare). Not confirmed → the grade
falls to SDP/NEW by the usual rules. Same-commit cycles and unverifiable cases (no clone)
are left alone. Lesson for DRIFT.4: relative `../` includes must be resolved;
a naive suffix loses real cycles.

## Completion date

2026-06-12

## Note on the commit

The scan code is in gitignored `experiments/` — only the report and the task
file go into the repository.
