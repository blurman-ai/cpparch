# [SCAN] Copypaste per-commit drift (research; precursor of the product new-clone-gate)

**Created:** 2026-06-11
**Started:** 2026-06-13
**Closed:** 2026-06-26
**Status:** done (product precision ≈86–91% obtained; `DRIFT.NEW_CLONE` #123 shipped)
**Module:** SCAN][DUPLICATION
**Priority:** major
**Difficulty:** unknown
**Blocks:** —
**Blocked by:** —
**Related:** #089 (boolean_state drift research), #077 (per_commit_graph_drift_export), #090 (boolean_state_drift_metric, future), #123 (product new-clone-gate in `--diff`)

> **MVP:** pulled into v0.1 — release blocker (decision 2026-06-13, [MVP.md](../../docs/MVP.md) §Acceptance #10). No longer post-release: per-commit copy-paste precision on the corpus is needed to know that there is real gold in the chest before publishing. The earlier "research, post-release" framing in the text below is superseded by this decision.

> **Status 2026-06-23:** product objective closed: precision of the product `DRIFT.NEW_CLONE`
> obtained (≈86–91%), the full 185-repo run was cancelled as not changing the decision. Remaining — board
> hygiene: move/format the task into `completed/`, not a dev blocker.

## PRODUCT precision obtained (2026-06-21)

The key MVP remainder is closed on the **product** detector (not the python MD5). Triage of
a stratified sample of 70 findings from the in-progress #124 run (25,062 new-clone
commits / 752 repos): **precision ≈ 86–91%**. Analysis: `experiments/per_commit/PRECISION_103.md`
(+ `nc_precision_triage.py`).

- 60/70 TP — genuine introduced copy-paste (block/function, 1-2 tokens changed);
  **0 false matches of unrelated code**.
- FP noise (6-10) — generated code (STM32CubeMX HAL, amalgamated single-header
  `epiworld.hpp`) + a vendored subtree (a nested copy of the project). These are EXACTLY the same
  exclusion-gaps #127/#129 found in #131; closing them will raise precision to ~95%+.
- Skeptic check: the first run of the harness gave 0 — a regex bug (`^` without MULTILINE), not
  the detector; fixed before drawing conclusions.

Recall / full fire-rate — from #124 (corpus run in progress). Together they close MVP criterion 10.

## Goal

Implement per-commit code duplication analysis (as a PR gate): track the addition of new clones in the commit history May2025–May2026 for 185 agentic projects, produce a CSV with per-commit duplication metrics.

## Context

Currently available:
- ✅ Graph-drift per-commit (9700+ records, `*_graph_drift.jsonl`)
- ✅ Boolean-drift per-commit (5514 commits, `bool_history_new185.csv`)
- ❌ Copypaste per-commit (only a HEAD snapshot in `EXAMPLES_50.md`)

A full PR gate needs all three layers in-window (May25–May26). Copypaste per-commit will allow blocking commits that add new code clones (per Juergens et al. ICSE 2009: 52% of clones are inconsistent → tech debt).

## Execution plan

- [ ] Write the script `copypaste_per_commit.py` (analogous to `bool_history_scan.py`)
  - For each commit in window (shallow-since=2025-05-01)
  - Scan the commit's **added lines**: new files IN FULL + added fragments
    in existing files (unified diff `--unified=0`). The "new files only"
    restriction is lifted: the most common clone scenario is a function copied next to it into
    an EXISTING file (exactly the inconsistent-clone case of Juergens), which only-new-files
    does not see
  - Compare the added code against the base (existing code before commit) for EXACT/RENAMED clones
  - Record: repo, sha, date, author, subject, dup_pairs_added, files_affected, target_kind (new_file | existing_file)
- [ ] CSV output: `copypaste_per_commit_new185.csv` (analogous to `bool_history_new185.csv`)
- [ ] Integrate into `RESUME_pending.sh` or a separate script
- [ ] Test on 2-3 repos (OloEngineBase, FastLED, llama.cpp)
- [ ] Merge with the existing `EXAMPLES_50.md` or a separate report

## Done

- **Steps 0–4**: wrote `copypaste_per_commit.py` — streaming `git log -p -U0`, rolling hash-index (`seen`), move filter (del_blocks per commit), CSV with fields `repo/sha/date/author_kind/subject/target_kind/dup_pairs_added/max_clone_tokens/files_affected/lines_added/example_file`.
- **author_kind**: determined via `%(trailers:key=co-authored-by)` + heuristics on author/subject (detects Claude/Copilot and the like).
- **Step 5 — synthetic test**: 3 correct detections (new_file copy 1 pair, full new_file copy 2 pairs, existing_file verbatim copy 1 pair), unique code and short copies (<6 lines) are not detected (as expected).
- **Eyeball on FastLED** (5 months, 3072 commits, 281 clone-commits): the move filter reduces FP from reorg (17→3 pairs on an FFT move). Found 2 classes of residual FP: (1) cross-commit refactoring (code removed in commit A, added in B — the move filter doesn't catch it), (2) boilerplate coincidental matches. Step 6 needs normalization by `lines_added` (pairs/KLOC).

## Reconnaissance result (2026-06-13)

**Decision: we do NOT complete the full scan of 185 repos.** On 22 heterogeneous repos (openwrt forks,
ML, embedded, a browser engine, llama.cpp) the order of magnitude settled — completing
the rest for the third significant digit makes no sense. The presentation numbers will anyway be taken
from the product **archcheck run**, not from this python MD5 detector.

The fraction of commits in which the detector fired ≥1 time (numerator — clone-commits from the CSV,
denominator — `git log --since=2025-05-01 --until=2026-05-31` over C++ files):

| Project | clone-commits | fraction |
|---|---|---|
| llama.cpp | 617 | 23% |
| react-native | 290 | 21% |
| nrf-sdk | 416 | 20% |
| FastLED | 876 | 19% |
| gtk | 397 | 12% |
| git | 145 | 6% |
| scylladb | 158 | 4% |
| bitcoin | 64 | 3% |

**Conclusion:** the fraction is single digits to twenties of percent, depending not on the agent (the
agentic-vs-human hypothesis already fell apart on the HEAD snapshot, p=0.144), but on the project culture.
bitcoin/git (strict code review) — 3–6%; fast feature projects — ~20%. Spread ×6–7.

**Three caveats (why these numbers are only an order of magnitude, not a product metric):**
1. This is the fraction of *commits with ≥1 firing*, not the volume of copied code (a commit with
   one 6-line match = a commit with 66 pairs).
2. Inside there is a known FP tail (cross-commit refactoring + boilerplate `namespace fl {…}`),
   so the "20%" are overstated.
3. The algorithm is a python MD5-over-6-lines, **not** the token-based archcheck detector;
   thresholds from here do not carry over to the product directly.

## Not doing (deliberately dropped)

- ~~Run a full scan over the 185 repos~~ — the order of magnitude is clear from 22, see above.
- ~~Step 6: agentic vs human clone-rate~~ — the hypothesis fell apart, the breakdown isn't needed.
- Calibration of product thresholds and presentation — **move to the archcheck run**
  (the product detector on diff scope), not to this research script.

## Detailed instruction (algorithm, functions, tests)

**Step 0 — reuse:** the commit/repo traversal scaffold from `bool_history_scan.py`
(iteration over `git log` in the window, CSV writer, RESUME idempotency); the token-based
clone detector — shipped C++ (`archcheck --duplication`) or the python wrapper
`clone_classifier.py` already in experiments.

**Step 1 — extracting the commit's added code** (`collect_added_code(repo, sha)`):
- `git show --unified=0 --diff-filter=AM <sha> -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp'`;
- for A files: the whole file = one added block (`target_kind=new_file`);
- for M files: hunks of `+` lines, glued into blocks by adjacency
  (`target_kind=existing_file`); blocks shorter than `MIN_TOKENS` (≈30, the shipped
  fragmenter threshold) are dropped immediately;
- vendor/test path filter — the same rules as `file_classification.h`
  (copy the list, pin it down in the script as a constant).

**Step 2 — base for comparison** (`base_index(repo, sha)`):
- the contents of `sha^` (`git ls-tree -r` + `git cat-file`) over the same extensions,
  the same vendor/test filter;
- index once per commit; for speed — cache the index between adjacent
  commits (invalidation by changed files).

**Step 3 — clone search**: each added block → candidates over the base → classification
EXACT/RENAMED (LITERAL/STRUCTURAL not counted — noise); **exclude self-match**
(don't compare a block against itself in the new version of the file; for M files compare the
added block against the `sha^` version, where it doesn't exist yet — this excludes the FP
automatically).

**Step 4 — output** `copypaste_per_commit_new185.csv`:
`repo, sha, date, author_kind(agentic|human|unknown), subject, target_kind,
dup_pairs_added, max_clone_tokens, files_affected, example_pair(fileA:line<->fileB:line)`.
`author_kind` — the same trailer/bot heuristics as in bool_history (needed for the
AI-vs-human breakdown, design #1 from the methodological critique).

**Step 5 — tests on 2–3 repos** (OloEngineBase, FastLED, llama.cpp):
- synthetic control: create a test repo of 3 commits — (a) a commit
  copies a function into an existing file → 1 existing_file pair; (b) a commit
  adds a new copy file → 1 new_file pair; (c) a commit adds unique
  code → 0 pairs. Run the script, check the CSV;
- eyeball the top-20 pairs of a real run: TP fraction ≥ 70%, otherwise raise MIN_TOKENS.

**Step 6 — analysis:** per-commit rate (pairs/KLOC added) agentic vs human commits
**within mixed repos** (repo fixed effects — the main design from the critique);
compare with the null result of the HEAD snapshot (p=0.144) — expectation: the per-commit
introduction rate is more sensitive, since vendored FPs are not "added" by commits.

## Next steps

1. Run `python3 copypaste_per_commit.py` overnight (185 repos, ~5–10 h, resume via `.done_repos`).
2. Step 6: load the CSV, build per-commit pairs/KLOC, compare agentic vs human within mixed repos (repo fixed effects), write it up in `experiments/FINDINGS.md` or a separate doc.

## Key decisions

| Decision | Reason |
|---------|---------|
| Per-commit, not per-diff | Tracking per-PR is needed (like graph/bool) |
| Added lines (new files + insertions into existing) | A clone is more often added into an existing file; only-new-files misses the Juergens scenario |
| Rolling `seen` hash-index (not git archive per commit) | One `git log -p` subprocess per repo instead of N git-show; performance ~0.2–1 s/commit |
| Move filter (del_blocks per commit) | Removes FP from reorg: del_hashes are excluded when checking added blocks |
| `lines_added` in CSV | Normalization to KLOC for step 6; needed because cross-commit refactoring creates outlier commits with >100 pairs |
| WARMUP_START = 2023-01-01 | Baseline for repos with history before May 2025; for new repos (0 warmup) seen is built within the window |

## Changed files

| File | Change |
|------|-----------|
| `experiments/boolean_state/copypaste_per_commit.py` | New script (streaming parser, move filter, resume, CSV) |
| `experiments/boolean_state/copypaste_per_commit_new185.csv` | Output (created on run) |
| `/tmp/test_copypaste_repo/` | Synthetic test repo (4 commits, not in git) |

## Notes

- May be slower than boolean (clone_classifier on every new file), but cheaper than graph-drift (not architecture)
- Baseline copypaste from the baseline snapshot (before May2025)
- **Path to product**: this script is the corpus validation of thresholds for the future
  product rule **new-clone-gate** (a clone ≥N tokens that has ≥1 instance
  entirely in the diff against baseline; see `docs/research/full_audit_and_research_2026_06.md`
  §5.2). In shipped code the token-based detector and baseline mechanics already exist — only the
  diff scope is missing. The "PR gate" title refers to this future phase; the task itself is research.

## SUMMARY (2026-06-26) — closed

The product goal is achieved: the token-based new-clone-detector in `--diff` is shipped as
`DRIFT.NEW_CLONE` (#123, in release 0.1.0), and its precision was measured on the corpus —
**≈86–91%** (triage of 70 findings). The full 185-repo run was cancelled as not carrying the decision
(precision is already defensible for advisory-first). The task played its role as the corpus validation
of thresholds and is closed; further precision/recall validation of the gate is carried out in #124.

- **How it works:** per-commit clone measurement on the fast backend, baseline snapshot + diff scope
  (a clone ≥N tokens, ≥1 instance entirely in the diff).
- **What controls it:** token/precision thresholds from corpus triage; the product entry point — `--diff`.
- **What it relates to:** precursor of #123 (shipped `DRIFT.NEW_CLONE`); validation — #124; corpus — #131.
- **Diagnostics:** `archcheck --diff` on a commit with copy-paste → line `new clone drift (advisory)`.
