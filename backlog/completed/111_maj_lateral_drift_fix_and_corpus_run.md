# [GRAPH] Lateral drift: pipeline refinement and corpus run of the criterion

**Created:** 2026-06-12
**Started:** 2026-06-12
**Status:** wip
**Module:** GRAPH][SCAN
**Priority:** major
**Difficulty:** unknown
**Blocks:** —
**Blocked by:** —
**Related:** #077 (per_commit_graph_drift_export), #103 (copypaste_per_commit_drift — the same research pipeline), #042 (clang_semantic_backend — bridge headers)

## Goal

Bring the per-commit graph-drift pipeline to a state where the criterion of lateral
cross-module drift ([docs/research/lateral_module_drift_criterion.md](../../docs/research/lateral_module_drift_criterion.md))
can be computed over the 481 corpus repos, and run it: how many LATERAL.{CYCLE,SDP,NEW}
remain out of 21,736 raw edge events.

## Context

The corpus check (2026-06-12) showed: 95% of records with `added_edges>0` are noise (activity,
not drift); the real signal is cycles (1%, of which 94 in small commits) and lateral edges
between peer modules. The criterion is designed and grounded in the literature (MacCormack shared
classification, Martin SDP, Lakos levelization, ArchLint heuristics, persistence per
Li/Liang) — see the research doc. The niche of a config-free edge-level criterion in the literature/tools
is not filled — potentially an original result + a candidate for the product rule DRIFT.4.

**Run blocker:** the criterion needs the state of the graph *before* the commit (FID/levels/instability),
while `*_graph_drift.jsonl` only has the deltas.

## Execution plan

### Phase 1 — pipeline fix

- [ ] **Spike (half a day):** find the minimal way to obtain the baseline graph:
  - can the current `archcheck` dump the full edge list of the include graph
    (json-reporter? a hidden flag?); if not — estimate a new flag `--dump-edges`
    (≤50 lines, see code_quality);
  - check the **completeness** of the `added`/`removed` lists in jsonl (the md report cuts to 12 lines —
    does it cut the jsonl?). If the lists are complete → the graph state at any commit
    is reconstructible by **incremental replay** from a single baseline snapshot,
    no need to touch C++.
- [ ] **Fix A — state before the commit:** a baseline snapshot at the first commit of the window +
      incremental replay of deltas in python (preferred), or a dump of the module
      summary of the parent from `generate_per_commit_graph_drift.py`.
- [ ] **Fix B — renames:** check how `archcheck --diff` treats `git mv`
      (a file moving between modules must NOT spawn a false "new pair" A→B);
      document the behavior, and add a rename filter in the scan if needed.

### Phase 2 — criterion prototype

- [ ] `lateral_drift_scan.py`:
  - auto-select module depth (the shallowest with ≥3 sibling sources,
    skipping wrapper directories); vendor/test filter as in `file_classification.h`;
  - on the "before-commit" state: FID/FOD over direct edges, Lakos level
    (SCC condensation + longest path), Martin I = Ce/(Ca+Ce);
  - shared classification: `FID(B) ≥ 0.5·max FID` ∧ `FOD(B) ≤ median` (high-in/low-out;
    high-in/high-out = Hub, not a legitimate target);
  - events: siblinghood, non-shared target, the first A→B pair, not mass-touch (≤150 edges),
    persistence to the end of the window; a grace period of the first m commits of a new module;
  - grades: LATERAL.CYCLE (a reverse B→A existed) > LATERAL.SDP (I(B) > I(A)+δ) >
    LATERAL.NEW;
  - CSV output: repo, sha, date, author_kind, grade, moduleA, moduleB,
    FID_B, level_A, level_B, I_A, I_B, example_edge.

### Phase 3 — run and evaluation

- [ ] Run over 481 repos; summary: events by grade vs 21,736 raw (expectation: hundreds).
- [ ] Eyeball top-30: TP share ≥ 70% (the bar as in #103), otherwise tune the thresholds
      (δ, the 0.5 shared threshold, the grace period).
- [ ] Cut agentic vs human within mixed repos (repo fixed effects,
      design like the boolean-drift one).
- [ ] Regenerate section A in `EXAMPLES_50.md` per the new criterion
      (drop single edges in Log.hpp, show LATERAL events).
- [ ] Results → `docs/research/lateral_module_drift_corpus_run.md`;
      decision on the product rule DRIFT.4 (CYCLE — gate candidate, SDP/NEW — advisory).

## Done

- **Spike (2026-06-12), results:**
  - jsonl: the `added` lists are **complete** (0 discrepancies counter/list across all 339,321 records);
    the `removed` lists are **absent** — `generate_per_commit_graph_drift.py` collected only
    the `added:` section. Scale of the loss: removed = 5.6% of edges, 1.1% of commits → replay of additions
    only yields ≤5.6% phantom edges, acceptable for a prototype.
  - `archcheck --save-graph-baseline` writes YAML `nodes[] + edges[[i,j]]` — parsed
    with regexes, no need to touch C++. `--format json` doesn't dump the graph (only violations).
  - **Blocker found:** 297/481 corpus repos are deleted from disk (184 remain in the flat
    `~/oss/<owner>_<name>`; the `_aidev_dense/` directory was retired). A run with
    a true baseline is possible only on the available ones; for the rest a re-clone is needed.
  - `git archive` hangs on asset-heavy repos (Alchemy) → extraction of only C++
    sources via `git ls-tree -r` + `git cat-file --batch` (Alchemy: 2677 files, ok).
- **`make_window_baselines.py`** — a batch: per repo takes `first_window_sha~1`, extracts
  the sources, runs `--save-graph-baseline`, writes `baselines/<name>_window_baseline.yml`
  + `baselines/manifest.tsv`. Accounts for old git without `ls-tree --format`.
- **`lateral_drift_scan.py`** — the criterion prototype: auto-depth modules, structural
  merging of parallel trees (`include/X` + `src/X` → module `X`, detected by ≥2 common
  children — without name-regex), FID/FOD/I/Lakos-level, grades CYCLE/SDP/NEW, seeding with the
  baseline graph (known pairs are not "first contact", established modules without a grace period).

- **Baselines manifest (1st pass):** 111 ok + 2 ok_empty; 297 repo_missing;
  70 "no_parent" turned out to be **shallow clones** with the boundary exactly at the first window
  commit → an off-by-one fallback added (baseline = the tree of the first commit, its
  own event edges are not produced — under-detection, not FP). The batch over them was re-run.
- **Fix B extended:** a target move (`src/compat.h → src/compat/compat.h`,
  Bitcoin-ABC core#25493 backport) is caught by a second heuristic — the from-file already had
  an edge to a file with the same basename. Confirmed on a real FP (38 → 35 events).
- **Grading of intra-commit cycles:** a pair A→B + B→A in one commit are now both
  CYCLE (previously the second direction got NEW — the commit's edges merged into the graph
  after detection).
- **Per-commit authorship:** `author_kind` = agent/human by BOT_HINTS (reused from
  `agent_author_scan.py`) via git log by the event sha; the repo-level flag is a separate
  column `repo_kind`.
- **Interim signal (91 repos with a baseline):** 35 events = CYCLE 14 / SDP 5 /
  NEW 16. Eyeball quality high: netdata `libnetdata→daemon` (an MCP commit!),
  domoticz `mcpserver→hardware`, Collabora `kit→windows`, KDE `libklookandfeel→kcms`
  (an incomplete extract). NEW on newborn libs (KDE) — healthy modularization,
  FP in spirit; CYCLE/SDP are clean here → confirms the gate/advisory split.

- **Baselines ready: 183** (114 exact + 68 off-by-one for shallow clones with the boundary
  at the first window commit + 1 empty root commit).
- **Final run (2026-06-12): 98 events** over 183 repos = CYCLE 43 / SDP 11 /
  NEW 44; raw `added_edges>0` on this subset — 10,617 → **108× suppression**.
  19 repos with events out of 183.
- **Eyeball top-30: TP ≈ 87%** (26/30) — the 70% bar met. FP classes: move with
  renaming (scylla), test code through the filter (impala `-test`/`testutil`),
  2× healthy consumption of a new library (both NEW; CYCLE/SDP clean).
- Along the way, 5 artifact classes were found and killed (mass-touch pairs, Apache banner
  → empty baseline at VPP, merging of foreign trees in xLights, include re-resolution,
  system basenames) — documented in the report §5.
- **Report: [docs/research/lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md)** —
  conclusions for DRIFT.4: CYCLE → gate, SDP/NEW → advisory; in CI mode both run crutches
  (baseline reconstruction, persistence) are not needed.
- Agentic cut: 12 agent / 86 human events (raw counts; normalization and
  fixed effects — the next step, the classification is conservative).

## In progress

- (empty — the main cycle of the task is done)

## Next steps

All loose ends closed (2026-06-12):

1. ✅ **#112** — section A in `EXAMPLES_50.md` (commit `a927f81`).
2. ✅ **#115** — re-clone of 296 repos + 479 baselines + full run + agentic cut
   (`5989ede`); the main conclusion: the agentic signal does not survive repo fixed effects.
3. ✅ **#113** — Apache banner ≠ vendor (`68437c0`).
4. ✅ **#114** — `-test`/`testutil` into the test filter (`362ca60`).
5. ⊘ Time-based grace period (30 days) — wasn't split into a task; #115 confirmed
   that NEW noise on the advisory grade doesn't block the gate → deferred into the product DRIFT.4.
6. ✅ **#117** (spawned from the #115 eyeball) — the CYCLE grader confirms the back-edge;
   corpus: CYCLE 153→146, 7 phantoms removed.

## Key decisions

| Decision | Reason |
|----------|--------|
| Incremental replay instead of archcheck on each sha | 481 repos × thousands of commits; regen of 43 repos took ~2 days — re-doing is unaffordable |
| Replay only added (removed lost) | There are no removed lists in jsonl; the cost — ≤5.6% phantom edges (the removed share by counters) |
| Baseline via ls-tree+cat-file, not git archive | archive hangs on asset-heavy repos; we extract only C++ sources |
| Merging include/X+src/X structurally (≥2 common children) | Without it depth=1 yields wrapper modules include/src/tools and 0 events; name-regex is forbidden by design |
| FID over direct edges, not transitive | MacCormack visibility is transitive → "infection" through chains of C++ headers (a conclusion of the lit review) |
| Relative shared threshold (50% of max FID) | MacCormack; solves the problem of monotone FID growth |
| Persistence only in the retro analysis | In the product's CI gate the edge is checked before merging — the condition falls away naturally |
| TP bar 70% on eyeball | The same as in #103 — a single standard for the drift-metrics wave |

## Changed files

| File | Change |
|------|--------|
| `experiments/ai_repo_run/lateral_drift_scan.py` | criterion prototype (gitignored) |
| `experiments/ai_repo_run/make_window_baselines.py` | building baseline snapshots (gitignored) |
| `experiments/ai_repo_run/lateral_drift_new.csv` | output, 495 events (gitignored) |
| `docs/research/lateral_module_drift_criterion.md` | criterion design |
| `docs/research/lateral_module_drift_corpus_run.md` | run report + §8 (full corpus) |

## How it works

The lateral drift criterion is computed over the per-commit graph-drift jsonl without re-running
archcheck on each commit: one baseline snapshot of the graph at the start of the window
(`make_window_baselines.py`) + forward-only replay of `added` deltas in python
(`lateral_drift_scan.py::IncrementalGraph`). On the "before-commit" state we compute
FID/FOD, Martin instability, Lakos level, and MacCormack shared classification; an event
is born on the first lateral link between peer modules that passes none of the filters (vendor/test,
mass-touch, rename, resolution artifact, system-basename, grace-period). Grades:
CYCLE (a module cycle is closed, the back-edge is confirmed by live sources — #117) >
SDP (a dependency on something less stable) > NEW (the first link). In CI mode for DRIFT.4
the run crutches (baseline reconstruction, persistence) are not needed — the parent graph
is taken from git, the gate stands before the merge.

## Outcome

**Status:** completed
**Completed:** 2026-06-12

The criterion is designed, grounded in the literature, implemented, and validated on the full
corpus (479/481 repos): **495 events** against 21,736 raw, eyeball TP 85%,
CYCLE precision 92% after #117. The main scientific result (#115): agentic
authorship does **not** increase lateral drift at the per-commit level — the raw predominance is
compositional, doesn't survive repo fixed effects; a clean before/after design on
this corpus is impossible (treatment×maturity are anticorrelated). A candidate for the
product rule DRIFT.4: CYCLE → gate, SDP/NEW → advisory.
