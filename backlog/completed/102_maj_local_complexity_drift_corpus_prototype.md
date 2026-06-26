# [RESEARCH][CORPUS] Local Complexity Drift Prototype On 100 Drift-Positive Repos

**Created:** 2026-06-10
**Started:** 2026-06-10
**Completed:** 2026-06-11
**Status:** completed
**Module:** EXPERIMENTS / CORPUS / DIFF / RESEARCH
**Priority:** major
**Complexity:** large
**Blocks:** —
**Blocked by:** —
**Related:** #079 (corpus_run_graph_dup_ai_correlation), #080 (manual_corpus_analysis_findings), #101 (local_complexity_drift)

## Goal

Build a **separate prototype project** that computes commit-level local complexity drift outside the shipped `archcheck`,
and run it over **100 local C/C++ repositories** that have already shown at least one current drift signal
in the existing corpus (`graph_errors > 0` or `dup_pairs > 0`).

The result is needed not for release as-is, but to answer three questions:

- does the function give a useful signal on real commits;
- where does it drown in noise;
- is it worth pulling into the shipped `archcheck` via #101 afterward.

## Context

- In [backlog/new/101_maj_local_complexity_drift.md](~/projects/cpparch/backlog/new/101_maj_local_complexity_drift.md)
  a product advisory signal is already recorded, but its direct integration into `src/` is too costly right now:
  there are open questions about diff runtime, JSON output and the report contract.
- The repo already has a ready corpus and artifacts that can be used without a new web collection:
  - [experiments/ai_repo_run/corpus_summary.tsv](~/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv)
  - [docs/research/cpp_ai_drift_report.md](~/projects/cpparch/docs/research/cpp_ai_drift_report.md)
  - [backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md](~/projects/cpparch/backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md)
- The current `corpus_summary.tsv` already gives a good positive pool:
  - total rows: `492`
  - with non-zero drift by current signals: `449`
  - `graph_only`: `81`
  - `dup_only`: `59`
  - `both`: `309`
- The user's framing here is not about yet another shipped check, but about a **fast external validation loop**:
  first verify the metric on the corpus and only then decide whether it deserves integration into the product.
- But before the corpus, an even cheaper validation layer is needed:
  first generate a **synthetic suite of typical complexity drift** and run the scorer on it,
  so as not to pour the raw metric straight onto 100 real repos.

## Scope

- The prototype lives **separately from the shipped runtime**:
  not in `src/`, not in `include/archcheck/`, not in `tests/` as part of the release binary.
- Acceptable format:
  - a self-contained subproject in `experiments/local_complexity_drift/`, or
  - a sibling prototype next to the repo, if that's more convenient dependency-wise.
- The following must be returned to the repository itself:
  - a sample manifest;
  - raw results;
  - a markdown report;
  - a short recommendation for #101: `ship / revise / drop`.

## The set

### Sample source

Take only locally available C/C++ repositories from
[experiments/ai_repo_run/corpus_summary.tsv](~/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv),
which have:

- `graph_errors > 0` or `dup_pairs > 0`;
- a live local git repo;
- at least one C/C++ file after the vendor filter;
- an available first-parent history within the corpus run window.

### Sample of 100 repos

So as not to turn the experiment into a top-100 of giants and not to cherry-pick by hand, take
a **deterministic stratified sample**:

- `25` repos from `graph_only`
- `25` repos from `dup_only`
- `50` repos from `both`

Within each bucket:

- split the repos by `cpp_loc` quartiles;
- take an equal number from each quartile;
- within a quartile, sort by `name` and take the first N.

If, after checking the local clones, a particular bucket doesn't have enough repos,
top up from `both`, keeping a record of the backfill in the manifest.

## What exactly we measure

Unit of analysis: **commit**.

On a commit we count:

- changed C/C++ files;
- matched functions / function-like bodies;
- old/new local complexity score;
- delta per function;
- aggregate per file;
- aggregate per commit.

Minimal finding shape:

- `repo`
- `sha`
- `parent`
- `file`
- `symbol`
- `old_score`
- `new_score`
- `delta_score`
- `delta_percent`
- `changed_loc`
- `branch_delta`
- `nesting_delta`
- `deep_lines_delta`

## Synthetic Test Pack

Before any corpus run, assemble a separate set of small baseline/current pairs that
represent **typical** local complexity drift and typical false positives.

Minimal set of scenarios:

- `flat_to_nested_if`
- `harmless_append_statement`
- `else_if_chain_growth`
- `switch_case_explosion`
- `loop_inside_loop`
- `lambda_nesting_growth`
- `guard_clause_refactor` as an example of complexity reduction
- `comments_and_strings_noise`
- `macro_heavy_file`
- `formatting_only_change`
- `function_move_without_growth`
- `extract_helper_reduces_complexity`

Each scenario needs an expected outcome:

- `must_trigger`
- `must_not_trigger`
- `should_reduce`
- `low_confidence_allowed`

The synthetic suite here matters no less than the real corpus:

- it quickly breaks a bad scorer;
- it gives a fixed regression set for the future #101;
- it lets us fix the matcher and noise filtering before the costly corpus run.

## Method

### Source of commits

Primary run mode:

- take only those commits that have already landed in the existing structural drift pipeline
  from #079;
- if a repo already has a `*_graph_drift.jsonl`, use it as the commit manifest;
- if a particular repo has no such file, fallback: first-parent commits since `2025-05-01`.

Secondary mode for a sanity-check:

- for each repo, add a small matched control set of zero-drift commits;
- this is needed to see right away whether the metric fires on any average commit.

### The metric itself

**The v1 formula (`branchScore + deepLinesCount + floor(indentComplexitySum / 20)`) was implemented,
run over the corpus and found defective** — an external review on 2026-06-11 with live repros
([docs/research/local_complexity_drift_scorer_review.md](~/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md)).
Do not use it and do not "fix it locally".

The v2 formula must replicate the "Scoring model" from #101 (cognitive-style, source —
[docs/research/cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) §4–§6):

- score = only structural increments (`+1 + control nesting`) and flat increments;
- `case`/`default` are NOT counted; `else`/`else if` = +1; a series of `&&`/`||` = +1; do-while = one count;
- nesting = control-nesting (classified braces), not brace-depth and not indentation;
- `deep_lines` / indent accumulation — only diagnostic fields in the output, not part of the score.

The goal is still not byte-for-byte Sonar compatibility, but a stable proxy portable into `archcheck` (#101).

### Technical approach

- Don't touch the `archcheck` binary and don't embed this into `src/main.cpp`.
- Do it via a separate runner that reads `git show`, `git diff --name-only`,
  `git diff -U0` and works on old/new contents without checking out the whole repo on each step.
- A pragmatic stack convenient for research is allowed:
  - Python 3
  - a small text-based parser
  - if needed, `tree-sitter-cpp` only as an auxiliary extractor
- But keep the scorer as close as possible to the planned product heuristic, rather than writing a different world.

## Concrete implementation plan

1. Create an isolated subproject:
   `experiments/local_complexity_drift/`
   with a minimum of:
   - `README.md`
   - `synthetic_cases/`
   - `requirements.txt`
   - `generate_synthetic_cases.py`
   - `run_synthetic.py`
   - `run_sample.py`
   - `select_sample.py`
   - `scan_commit.py`
   - `analyze_results.py`
2. First assemble the synthetic drift suite:
   - baseline/current pairs of small `.cpp/.h` files;
   - a manifest with the expected outcome;
   - a separate runner that runs the scorer only over the synthetic cases;
   - a text summary that immediately shows `expected vs actual`.
3. Implement `scan_commit.py`, which for one `repo + parent + sha` or for a synthetic pair:
   - finds changed C/C++ files;
   - gets old/new text via `git show` or from the synthetic case;
   - cuts out noise: comments, string literals, blank lines;
   - finds function-like bodies;
   - counts old/new metrics;
   - returns per-function and per-commit JSON.
4. Separately implement a conservative function matcher:
   - first `file + symbol`;
   - fallback `file + nearest line anchor`;
   - if a match is unstable, mark the finding as `low_confidence` rather than guessing.
5. Run the synthetic suite and don't go to the corpus until the basic expectations are met:
   - `must_trigger` actually trigger;
   - `must_not_trigger` stay silent;
   - `should_reduce` reflect a score reduction;
   - noise from comments/strings/macros doesn't dominate.
6. Only after the synthetic pass, in `select_sample.py` read
   [experiments/ai_repo_run/corpus_summary.tsv](~/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv),
   build the positive pool and emit
   `sample_100.tsv` with the columns:
   - `name`
   - `repo_path`
   - `bucket`
   - `cpp_loc`
   - `graph_errors`
   - `dup_pairs`
   - `ai_pct`
   - `selected_by`
7. For each repo, learn to get the commit list without a checkout loop:
   - from the existing `*_graph_drift.jsonl`, if the file already exists;
   - otherwise from `git rev-list --first-parent --since=2025-05-01 HEAD`.
8. In `run_sample.py` make a resumable batch run:
   - manifest-in
   - per-repo progress
   - append-only jsonl outputs
   - retries on broken repos
   - parallelism across repos
9. Collect three layers of output:
   - `raw/findings.jsonl` — all findings
   - `raw/commit_summary.jsonl` — one row per commit
   - `reports/sample_100_summary.tsv` — one row per repo
10. In `analyze_results.py` join the results with the existing corpus context:
   - `graph_errors`
   - `dup_pairs`
   - `ai_pct`
   - repo size
   - the number of drift-positive commits
11. Emit the final markdown report:
   `docs/research/local_complexity_drift_corpus_report.md`
   with sections:
   - synthetic suite results
   - coverage
   - top findings
   - obvious false positives
   - correlation hints
   - recommendation for #101
12. At the end, record a short decision memo:
    - `ship`: if the signal is stable and gives useful review cues;
    - `revise`: if the idea is alive, but the formula/threshold/mapper are noisy;
    - `drop`: if the noise floor is too high.

## What counts as success

- There is a synthetic drift suite with expected outcomes.
- The scorer first passes the synthetic suite.
- There is a deterministic `sample_100.tsv`.
- The prototype doesn't reach into the shipped `src/` and doesn't change the `archcheck` contract.
- Raw per-commit results are obtained over the 100 repos.
- There is at least a basic control slice over zero-drift commits.
- There is a report with top true-positive / false-positive cases.
- There is an explicit decision on how this affects #101.

## Readiness criteria

- Synthetic cases are stored separately and run reproducibly with a single runner.
- The `must_trigger` / `must_not_trigger` / `should_reduce` scenarios give the expected result.
- The prototype runs reproducibly from a manifest, not by hand over individual repos.
- The sample is really assembled from the drift-positive pool, not from arbitrary favorite repos.
- There are raw JSONL/TSV results and a markdown summary.
- There are at least 10-20 cases examined by eye, not just a dry aggregation.
- The report records separately:
  - where the signal is useful;
  - where it is noisy;
  - whether the scorer is portable to the product almost as-is or not.

## Do not do

- Don't pull this straight into `archcheck --diff`.
- Don't break `rules::Violation`, `json_reporter.cpp` or the config schema for the sake of the experiment.
- Don't make a multi-language version right away.
- Don't try to prove causality `AI -> complexity drift` at this step.
- Don't smear the scope over the whole corpus of 492 repos, if 100 is already enough for a decision.

## Done

- Created an external prototype project:
  [experiments/local_complexity_drift/](~/projects/cpparch/experiments/local_complexity_drift/README.md).
- Implemented a text-based scorer:
  [scan_commit.py](~/projects/cpparch/experiments/local_complexity_drift/scan_commit.py).
- Generated a synthetic suite of 13 typical baseline/current cases:
  `flat_to_nested_if`, `else_if_chain_growth`, `switch_case_explosion`, `loop_inside_loop`,
  `lambda_nesting_growth`, `guard_clause_refactor`, `comments_and_strings_noise`,
  `macro_heavy_file`, `formatting_only_change`, `function_move_without_growth`,
  `extract_helper_reduces_complexity`, `large_linear_body_growth`.
- The synthetic runner passes the whole suite:
  `13/13 passed`.
- Recorded a decision on LOC-only growth:
  a large increase of meaningful LOC inside an existing function is not in itself a finding.
  It is context for review, but not `archcheck`'s zone; if the linear length of a function matters, that should be caught by
  a static analyzer / a function-length metric.
- Assembled a deterministic `sample_100.tsv` from the local positive pool:
  - `100` repos;
  - `both`: `67`;
  - `dup_only`: `25`;
  - `graph_only`: `8`;
  - `graph_only` had to be under-filled due to the actually available local clones/manifests.
- Ran a corpus run over all 100 repos of the sample with protective limits:
  - command:
    `python3 experiments/local_complexity_drift/run_sample.py --max-repos 100 --max-commits 20 --max-files-per-commit 30 --max-file-bytes 300000 --reset-output`
  - scanned commits: `1614`;
  - findings before author-code filtering / existing-function-only semantics: `8521`;
  - findings after porting the shipped vendor/test filtering and cutting off new functions: `524`;
  - errors: `0`;
  - skipped oversized/excess/filter-excluded files: `9608`.
- Found and ported the current shipped filtering from:
  [include/archcheck/scan/file_classification.h](~/projects/cpparch/include/archcheck/scan/file_classification.h)
  and [src/scan/project_files.cpp](~/projects/cpparch/src/scan/project_files.cpp):
  vendored dirs, test dirs, vendored basenames, license-header heuristic.
- Produced a report:
  [docs/research/local_complexity_drift_corpus_report.md](~/projects/cpparch/docs/research/local_complexity_drift_corpus_report.md).
- Produced a separate report with concrete before/after examples:
  [docs/research/local_complexity_drift_examples.md](~/projects/cpparch/docs/research/local_complexity_drift_examples.md).
- Performed a manual review of all 6 generated before/after examples:
  - clear TP: `6`;
  - clear FP: `0`;
  - caveat: `2` UI/settings renderer cases, branch-heavy by nature but still real in-function growth.
- Regenerated the corpus report after removing the LOC-only trigger:
  - scanned commits: `1614`;
  - findings: `524`;
  - reason counts:
    `complexity_delta=477`, `zero_to_complex=32`, `relative_complexity_delta=12`,
    `body_and_complexity_growth=3`;
  - the `large_linear_body_growth` synthetic case stays silent: `must_not_trigger`.
- Added an analyzer:
  [analyze_results.py](~/projects/cpparch/experiments/local_complexity_drift/analyze_results.py).
- Recorded the preliminary conclusion for #101:
  `revise`, not `ship` as-is.

### v2 scorer (2026-06-11) — the review is worked through

- **Rewrote `scan_commit.py` into a Sonar Cognitive Complexity token scanner**
  (design — [cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) §4/§6):
  a brace stack with control/non-control classification, a lastOp stack of `&&`/`||` series by
  bracket depth, do-while pairing, an `else`/`else if` hybrid, lambda nesting, braceless bodies.
  The line-by-line parsing of v1 is replaced entirely — an `&&` series split across lines is now
  counted once (that was the root of the FP on condition reformatting).
- **All 7 review defects closed**, verified on `review_repros/` (a 0→1, b Δ0, c 0,
  d Δ0, e 0→1, f 2→2). A resolution section was appended to
  [scorer_review.md](~/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md).
- **D6/D7**: a blacklist of test symbols `TEST*/BENCHMARK` in `signature_symbol`, a suffix filter
  for `*tests` directories, top-level arity in the match key (`(symbol, arity)`).
  Effect: TEST findings in the corpus 0 (was 2 in the top-20), low-confidence 43→21.
- **`finding_reason` → an LCX hierarchy**: `crossed_25` (LCX.1) / `grew_when_already_above`
  (LCX.2) / `complexity_delta` Δ>=5 (LCX.3, non-strict). Without normalization to diff size.
- **Synthetic suite 13/13** under v2; `switch_case_explosion` re-labeled to
  `must_not_trigger` (this is defect D1, cognitive delta 0), `else_if_chain_growth`
  triggers at the boundary Δ=5=K.
- **Corpus re-run** with the same command: 1612 commits, **403 findings** (v1: 524).
  Reasons: `grew_when_already_above`=210, `complexity_delta`=127, `crossed_25`=66.
  The top — genuine complexity growth on a sane scale; switch parsers and TEST_F are gone.
  **6/6 of the manual TPs survived** (3× crossed_25, 3× grew_when_already_above).
- Reports regenerated: corpus_report (v2 numbers + comparison with v1), the examples doc
  (scores moved to the v2 scale + a note about 6/6 survival), analyze_results.py
  (noise-notes and recommendation under v2).
- **The recommendation for #101 refined**: `revise`, leaning `ship` for the scorer core —
  the metric is ready to port (authority-backed, 13/13, D1-D7 closed, 6/6 TP), but before
  a default rule a threshold floor on LCX.2 is needed (the Δ1-2 tail on already-huge functions,
  72/210, non-actionable) and a confirmation of K=5 on a second corpus slice.

### Manual review round-2 (2026-06-11) — the 10-20 case threshold is closed

- Reviewed a **second round of 10 cases**, deliberately from the noisy strata (not top-TP):
  the Δ1 tail of `grew_when_already_above`, low-confidence matches, zero-baseline functions,
  medium deltas. Across the two rounds — **16 cases** with labels
  ([examples.md](~/projects/cpparch/docs/research/local_complexity_drift_examples.md),
  the "Manual Review Round 2" section).
- Result: `10` actionable TP, `3` non-actionable TP (Δ1 tail / mechanical guards),
  `2` FP, `1` low-confidence confusion.
- **Found 2 genuine FPs**: `__attribute__((unused))` taken for a function signature
  (a parser mismatch of the text fragmenter); `_pack_shoal_waves` Δ1 — a phantom +1 from
  the heuristic "`!` breaks the series" when a condition was simplified. Both — small-Δ or obviously false.
- **The key v2 validation**: `process_connection` 574→681 — the growth is entirely from nested
  `if/else` inside new `case`s, and NOT from the number of case labels (in v1 such switch dispatchers
  dominated the top by the number of labels). v2 measures branching depth, not dispatcher size.
- Added triage buckets in `analyze_results.py`:
  `high_confidence_existing_growth`=354 / `zero_baseline_existing_growth`=28 /
  `low_confidence_match`=21.

## How it works

**The prototype** (`experiments/local_complexity_drift/`, outside the shipped runtime, in `.gitignore`)
computes commit-level growth of local complexity and validates the metric before porting into #101.

- **The scorer** (`scan_commit.py`, v2): a **Sonar Cognitive Complexity** token scanner
  (Campbell 2018) — linear code = 0 by construction. Single-pass over the tokens of the cleaned
  function body:
  - structural (`if`/`for`/`while`/`switch`/`catch`/`?:`) = `+1 + nesting`;
  - hybrid (`else`/`else if`) = `+1` without a nesting bonus, but raises the nesting;
  - fundamental (`goto`/`co_await`, each `&&`/`||` series) = `+1`;
  - `case`/`default` are free, `switch` = +1 once; do-while = one count;
  - nesting — a classified brace stack (control vs class/namespace/init/lambda),
    not brace-depth and not indentation; series of logical operators — a lastOp stack by `(` depth;
  - rvalue `&&` (`Type&&`) is filtered out by "stickiness" to the token; `!=`/`==`/`<=`/`>=`
    are tokenized whole, so that `!=` doesn't look like a logical NOT.
- **Function matching**: the key `(symbol, arity)` — overloads of one name don't get confused;
  test symbols `TEST*/BENCHMARK` and `*tests` directories are filtered out; ambiguous ones —
  `low_confidence`.
- **Signals** (`finding_reason`, the LCX hierarchy): `crossed_25` (crossed the threshold 25) /
  `grew_when_already_above` (grew when already above 25) / `complexity_delta` (Δ≥5, non-strict).
  Without normalization to diff size.
- **Pipeline**: `select_sample.py` → a deterministic `sample_100.tsv` from the drift-positive
  pool; `run_sample.py` → a resumable batch over 100 repos (git show old/new, vendor/test
  filters, limits); `analyze_results.py` → aggregates + corpus_report.
- **Validation ladder**: `generate_synthetic_cases.py`/`run_synthetic.py` (13 typical
  drift pairs, the gate before the corpus) → `review_repros/` (6 repros of defects from the external review) →
  the corpus of 100 repos → a two-round manual review of 16 cases.

**The decision for #101**: `revise`, leaning `ship`. The core (cognitive complexity) is ready; before
a default rule — threshold work, not scoring work: a floor/down-rank on `grew_when_already_above`,
down-rank `low`-confidence out of the headline, a blacklist of `__attribute__`/`__declspec`.

## Key decisions

| Decision | Reason |
|----------|--------|
| A separate prototype project | The usefulness of the metric must be checked without embedding it into the shipped runtime |
| Synthetic suite first, corpus later | Otherwise it's too easy to drown in noise and not know whether the scorer is broken or the code is genuinely complex |
| 100 repos from the positive pool, not the whole corpus | For a research decision this is enough and substantially cheaper |
| A deterministic stratified sample | Otherwise the experiment is unrepresentative and unreproducible |
| Commit-level, not just HEAD-vs-baseline | The user's framing is exactly about drift on a commit |
| Keep the scorer close to #101 | Otherwise the result ports badly into the product |
| A separate report with a decision memo | A clear exit is needed: ship / revise / drop |

## Planned files

| Area | Change |
|------|--------|
| `experiments/local_complexity_drift/README.md` | Description of the prototype and how to run it |
| `experiments/local_complexity_drift/synthetic_cases/` | Small baseline/current pairs of typical drift |
| `experiments/local_complexity_drift/generate_synthetic_cases.py` | Generation of the synthetic suite |
| `experiments/local_complexity_drift/run_synthetic.py` | Running the scorer over the synthetic suite |
| `experiments/local_complexity_drift/select_sample.py` | Assembling `sample_100.tsv` |
| `experiments/local_complexity_drift/scan_commit.py` | Scorer of a single commit |
| `experiments/local_complexity_drift/run_sample.py` | Batch run over 100 repos |
| `experiments/local_complexity_drift/analyze_results.py` | Aggregation and summaries |
| `experiments/local_complexity_drift/sample_100.tsv` | The fixed sample |
| `docs/research/local_complexity_drift_corpus_report.md` | The final report |
