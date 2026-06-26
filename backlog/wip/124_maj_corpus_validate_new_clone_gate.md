# [SCAN][DUPLICATION] Corpus validation of the new-clone-gate: archcheck --diff over a sample of clone commits

**Creation date:** 2026-06-13
**Start date:** 2026-06-14
**Status:** wip
**Module:** SCAN][DUPLICATION
**Priority:** major
**Difficulty:** medium
**Blocks:** —
**Blocked by:** #123 (needs a shipped --diff new-clone-gate + parent-guard)
**Related:** #123 (product detector+gate), #103 (the Python recon localized the clone commits → the sample's source)
**Verification:** #131 (Group 2: Part B — fire-rate `DRIFT.NEW_CLONE` across the whole corpus)

## Goal

Obtain **product** precision/recall numbers for the new-clone-gate by running
the shipped `archcheck --diff` on a **sample of real clone commits** from the corpus — not
with the Python MD5 detector (#103) and not via a replay of the whole history. These same numbers go into
the presentation.

## Why this is the right path (the "walk vs real PR" fork is a false one)

At first it seemed there were two options: (a) build an incremental corpus-walk
(process every commit cheaply), or (b) wait for real PRs (endlessly long).
**Neither is needed.**

Key: in CI, git itself **lays out the full code** on disk (`actions/checkout` →
working tree), and `archcheck --diff before..after` reads the ready tree
(`DiskFileSource`, current=WORKTREE) + git objects for the parent. archcheck **does not
reconstruct a snapshot** — the code is fed "from outside". So validation = emulate
exactly this on a sample of clone commits:

- the repos are already on disk (`~/oss`, with `.git`) → `--diff-mode=memory`
  reads both states from objects, **without checkout**;
- `archcheck --diff (N-1)..N` over dozens of clone commits = **minutes**, not dozens of
  hours;
- processing EVERY commit isn't needed (a sample is needed) → the incremental walk is
  redundant (YAGNI). Experiment 1 (`experiments/incremental_state_check.py`,
  gitignored) proved that incremental state reconstruction is accurate
  (0 mismatch / 304 commits, ~45× less fragmentation, DF — an exact additive
  counter, no approximation) — the mechanism works, but isn't required for this purpose.

## Execution plan

- [x] Sample source: from the #103 CSV — `gen_sample.py`, stratification by
      target_kind × token-bucket, 109 commits / 20 repos.
- [x] For each: `archcheck --diff <sha>^..<sha> --diff-mode=memory --format=json`
      via a resumable parallel runner (`run_worklist.py`), collect `DRIFT.NEW_CLONE`
      + all other categories.
- [x] Summary + eyeball + cross-check with Python (`analyze_sample.py`).
- [x] Numbers → `experiments/FINDINGS.md` (Part A).

## Progress (2026-06-14)

**Harness:** `experiments/per_commit/` (gitignored) — one thin runner over the
shipped binary, no check rewritten. `archcheck --diff` bundles ALL
categories (graph-gating + graph-drift + complexity + new-clone + SATD + test-coevo)
in one call. Scripts: `gen_sample.py`, `run_worklist.py`, `analyze_sample.py`,
`gen_full.py`, `launch_full.sh`, `status.sh`.

**Validation (#124, sample of 109 commits):**
- The pipeline is robust: 109/109 without crashes; all categories fire per-commit
  (except grown-cycles — a sample of clone commits doesn't provoke them, covered by fixtures).
- new-clone fires on 17/108 eligible Python clone commits (16%), growing to
  **40%** on existing_file >300 tokens — the product detector is strictly **cleaner** than
  the Python MD5-over-6-lines (as expected).
- Eyeball: TP confirmed (FastLED `02274f1a5` — 10 EXACT copies, spans literally
  verified); the largest silent (gtk `25c7bd54` — split/move) = a correct
  non-detection + a Python FP; most silents = diff-scope artifacts of Python
  (added=0/merge/fork-history).
- Found a gap: the diff **JSON** doesn't report the bulk-import-skip fact (#117) — only text.
  The runner works around it via `git numstat`; the product ought to put a marker in JSON.

**Scope expansion (user request 2026-06-14):** launched a **full-corpus**
per-commit run of ALL checks — 1,051,194 commits / 1,685 repos, C++ since 2024-06,
`--no-merges`. Detached (`setsid nohup`, 64 workers, 120s timeout, slow-repo
blacklist). Bottleneck = process spawn (CPU idles), not CPU/RAM. ETA of the full
pass ~1.5–2.5 days, resumable. Part B numbers in `experiments/FINDINGS.md`.

## Fresh run on the current binary (2026-06-20)

The previous full run (`worklist_light`, 520177 commits) finished 18.06, but on the
**old binary** — before the full landing of #127/#129 (vendored/generated exclusion
changes the set of scanned files → new-clone and graph fire-rate shift) and before
the rebuild. Old results saved: `results_full.oldbin_20260618.jsonl`.

**Restarted from scratch on HEAD `4ec4445`** (debug binary, runner default):
`launch_full.sh 8 60`, worklist_light (520177), detached/resumable, baloo suspended.
The start is healthy: done is growing, all categories fire (`new_clone`/`graph_edges`/`complexity`/
`bulk_skip`), parentless-guard 1307 skip. ETA @8w ~5.5–6.5 days (cheapest-first →
1000-floor early). Monitoring: `status.sh`; stop: `os.killpg` of the PID from `run_full.pid`.
Heavy corpus (`worklist_heavy.tsv`, 226060) — separately after light.

## Next steps

- [ ] Wait for corpus coverage, write the Part B summary (corpus-wide fire rates
      by category, the share of slow-repo-skip, top findings) — ON THE CURRENT binary.
- [x] product: bulk-import-skip marker in the diff JSON (`complexity_skipped_added_lines`)
      — done this session (DiffJsonContext → diff_json_report.cpp, 2 e2e tests,
      528/528 green, dogfood 0). Closed the item in `backlog/DEBT.md`.

## Operations of the long run (important for resume)

- Launch/resume: `bash experiments/per_commit/launch_full.sh [workers] [timeout]`
  (default **8 workers**, 60s — the user's machine, don't oversubscribe cores, see
  [[project_archcheck_diff_git_orphans]]).
- Status: `bash experiments/per_commit/status.sh`. Stop: `os.killpg` of the PID from
  `run_full.pid` + finish off archcheck/git by PID (pkill is blocked).
- During the run: `balooctl suspend` (transient, may resume; at
  ≤8 workers it's harmless). `--diff-mode=memory` doesn't do a checkout → the oss trees
  on disk don't change.
- ETA of the full pass @8w ~10–11 days (throughput ~1.1/s); resumable, 347 slow
  repos already pre-blocked.

## Nuance for real CI (for #123 Stage 2)

`actions/checkout` by default is **shallow (fetch-depth: 1)** — the parent blob is
unavailable, the diff is empty. In the workflow you need `fetch-depth: 2` (PR) or `0` (push on
a range). Lock this down in the example workflow.

## Key decisions

| Decision | Reason |
|---------|---------|
| A sample of clone commits, not the whole history | for validation/numbers a representative sample is enough; replaying the whole history — dozens of hours wasted |
| `--diff-mode=memory` (from `.git`, no checkout) | the repos are already on disk; don't proliferate worktree materializations per commit |
| The product `archcheck --diff`, not Python | the Python MD5 detector ≠ the shipped token one; the numbers must be product numbers |
| Incremental walk canceled | a sample is needed, not every commit → the mechanism is redundant (proven working, but not needed) |

## Changed files

| File | Change |
|------|-----------|
| `experiments/` (gitignored) | wrapper script: sample → `archcheck --diff` per-commit → summary |
| `experiments/FINDINGS.md` | product numbers of the new-clone-gate on the corpus |
| `src/git/git_object_file_source.cpp`, `.h` | **empty-blob desync fix** (awaits `/commit`) |
| `tests/unit/git/git_object_file_source_test.cpp` | test: an empty blob doesn't desync the batch |

## Autonomous long run + 2 bugs (2026-06-14)

The long per-commit run of the whole corpus (1200 cheap repos, 520k commits, all
categories, memory mode) revealed **two correctness bugs**, both caught by the skeptic-check
before showing the numbers:

1. **Parentless commits (a harness artifact).** The corpus is cloned shallow → the boundary
   commit is parentless → `archcheck --diff sha^..sha` diffs against an empty tree → the whole
   repository is "added" → ~50-62% of graph findings are fake. Fix: skip parentless
   (`--min-parents=1` + a precompute-guard in the runner). The ledger was cleaned out.

2. **empty-blob desync (a PRODUCT bug, dogfooding).** `GitObjectFileSource::read()` on a
   0-byte blob didn't read out the trailing `\n` from `cat-file --batch` → stream shift →
   a corrupted include graph in memory mode (memory 102/118/3 vs disk 0/0/0 on a commit that
   didn't touch cyclic files). Fix: `parseBlobSize` → `optional<size_t>`. Verified
   memory==disk on 3 commits, 530/530, +unit test. **Awaits `/commit`** (product).

The run was restarted from scratch with both fixes. Graph categories are now reliable; the
numbers — after accumulation. Evidence: `experiments/FINDINGS.md` (Part B, fix #1/#2); open
product questions: `backlog/DEBT.md` (silent empty-baseline; graph vs bulk-gate).
