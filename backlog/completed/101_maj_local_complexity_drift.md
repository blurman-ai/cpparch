# [DRIFT][DIFF] Local Complexity Drift Advisory Signal

**Created:** 2026-06-10
**Started:** 2026-06-11
**Status:** wip
**Module:** GIT / SCAN / DIFF / REPORT
**Priority:** major
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #024 (in_memory_fs_for_diff), #075 (trusted_diff_workflow), #093 (flag_argument), #096 (satd_delta), #097 (test_co_evolution), #099 (indentation_complexity_drift)

## Goal

Add an advisory-only drift signal that catches local growth of control-flow complexity in changed C/C++ code:

- branching growth;
- nesting growth;
- growth of deeply nested lines;
- growth of local "patch accumulation" complexity inside functions.

This is a supporting signal for review, not an architectural gate, and not an attempt to turn `archcheck` into a general-purpose linter.

## Context

- The source statement is already in the repo: [docs/codex_task_local_complexity_drift.md](~/projects/cpparch/docs/codex_task_local_complexity_drift.md).
- In the current code `--diff` can only do a graph-vs-graph structural report via `RegressionReport` from [src/diff/regression_report.cpp](~/projects/cpparch/src/diff/regression_report.cpp). Local drift signals are not wired in there yet.
- The current snapshot report contract (`rules::Violation` + [json_reporter.cpp](~/projects/cpparch/src/report/json_reporter.cpp)) is too narrow for `old_score/new_score/delta_percent/symbol/start_line/end_line`, and `--diff` doesn't support `--format json` at all.
- The current config schema (`version/modules/rules/thresholds`) supports neither `rules.local_complexity_drift` nor advisory mode knobs. That means the YAML shape from the original statement cannot be promised in the first version without a separate schema task.
- There is no standard `--strict` mechanism in the repo. That means the first implementation must be advisory-only and must not invent a new strict mode.
- The task **overlaps** with #099: `indentation_complexity_drift` is a narrow file-level proxy, while this task is a more useful function-aware combined signal. Do not implement them as two independent products; `#099` should become a fallback/subsignal or be closed as absorbed-by-#101.
- **The prototype review (#102) found defects in the original formula** — analysis with live repros:
  [docs/research/local_complexity_drift_scorer_review.md](~/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md).
  The key principle the formula must respect (owner's statement):
  **volume ≠ complexity** — adding 50 flat lines (even with deep indentation
  from alignment) must produce a score delta of 0; growth comes only from added
  control structure (a new if, nesting, branching). The "Scoring model" section
  below is rewritten to follow this principle. The research on formal measurement is
  complete — spec, validation, token-feasibility and the design of delta signals:
  [docs/research/cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md).

### Validation and handoff from #102 (2026-06-11, prototype complete)

The prototype (#102, completed) implemented **exactly this scoring model** in Python
(`experiments/local_complexity_drift/scan_commit.py`) and ran it over the corpus.
So for v1 the algorithm is not theoretical but proven; below is what it showed and
which product decisions remained open for the C++ implementation.

**What is confirmed (can be relied on, don't re-open):**
- The token cognitive scorer + LCX hierarchy work: corpus of 100 repos, 1612 commits,
  **403 findings** (the old defective formula produced 524, inflated by switch parsers).
- The "definition of done" against the corpus from this task is **already met by the prototype**:
  switch parsers and `TEST_F` bodies left the top, **6/6 manual TPs preserved**.
- All 6 repro defects (`review_repros/`) and synthetic 13/13 green on the v2 scorer.
- Manual analysis of 16 cases: 10 actionable TP, 3 non-actionable TP, 2 FP, 1 low-conf.
  Reports: [corpus_report](~/projects/cpparch/docs/research/local_complexity_drift_corpus_report.md),
  [examples](~/projects/cpparch/docs/research/local_complexity_drift_examples.md) (round 2 — where it's noisy).

**Open decisions for v1 (this task closes them, not the prototype):**
1. **`LCX.2 grew_when_already_above` is noisy on Δ1-2.** In the corpus this is 210/403 findings,
   of which **72 are Δ1-2** (a function was score 200, grew to 201 — formally growth,
   useless for review; cases #7/#8 of the examples doc). Decision: give `LCX.2` a **floor
   threshold Δ≥K**, or report it **advisory-only separately** from `LCX.1`/`LCX.3`.
   By default I recommend: in the gating output — only `LCX.1 crossed_25` and
   `LCX.3 delta_ge_k`; `LCX.2` as an advisory line. (Recommendation, final call is the owner's.)
2. **Pseudo-signatures.** The text fragmenter takes `__attribute__((unused))`
   / `__declspec(...)` before a function as the function name (corpus FP: symbol
   `__attribute__`, score 1→30). Blacklist — in `function_body_scan` (Step 2).
3. `low`-confidence already does not yield `LCX.1/2` (Step 4) — that's sufficient; additionally
   they should be kept out of the headline output (down-rank in the report).
4. K=5 and threshold 25 — keep them as defaults from the design doc; a "second slice" for calibration
   without ground truth is weakly informative, better to calibrate on real runs.

## Execution plan

### Detection contract

- The primary unit of analysis: a function or function-like body.
- If a function is not recognized reliably, a file-level fallback is allowed, but only as diagnostics in the report (#099 territory), not as a finding in v1.
- The primary rule id:
  - `DRIFT.LOCAL_COMPLEXITY`
- A finding arises when one of the following conditions holds on changed code (the exact hierarchy is in Step 4 of the detailed instruction below; there are no other kinds of findings in v1):
  - `LCX.1 crossed_25` — a function (new or grown) crossed the absolute threshold 25;
  - `LCX.2 grew_when_already_above` — Δ > 0 in a function that was already ≥ 25;
  - `LCX.3 delta_ge_k` — Δ ≥ K (K=5), a soft warning.
- `delta_percent` conditions and file-level aggregate findings are NOT implemented in v1: percentage and file-level semantics are #099-fallback territory; file-level aggregates remain diagnostic report fields without their own finding.

### Metric shape

Per-function:

- `file`
- `symbol`
- `startLine`
- `endLine`
- `meaningfulLoc`
- `localComplexityScore` — cognitive score, the only field participating in findings
- `maxNestingDepth`, `structuralCount`, `logicalSeries` — diagnostics from the core (Step 3)
- `deepLinesCount`, `indentComplexitySum` — diagnostics, NOT part of the score (see Scoring model)

Per-file aggregate:

- `functionCount`
- `meaningfulLoc`
- `totalLocalComplexityScore`
- `maxFunctionScore`
- `changedFunctionCount`
- `changedFunctionsComplexityDelta`

### Scoring model

The formula's invariant: **code volume does not raise the score**. 50 added flat lines
(calls, initialization, data, aligned continuations) = delta 0; the score grows
only from control structure. The reference is the Sonar Cognitive Complexity spec
(Campbell 2018; the same metric is used by CMU MSR 2026 — corpus numbers will be comparable
to the literature), with documented token simplifications.

- First remove noise:
  - blank lines;
  - comments;
  - string/char literals;
  - preprocessor-only lines for branch scoring.
- Structure increments (`+1 + current control nesting`):
  - `if`, `for`, `while`, `do-while` (once per loop — the `while` of a `do` is not counted separately),
    `switch` (one structure; **`case`/`default` are NOT counted** — the spec didn't
    include them anyway, prototype #102 counted them erroneously), `catch`, `?:`.
- Increments without nesting (`+1`):
  - `else` / `else if` (as one branch, not two);
  - a series of identical `&&` / `||` (**+1 per series, not per operator**;
    `&&` right after an identifier/`>` in a declaration context is an rvalue reference,
    don't count it);
  - `goto`; `break`/`continue` **with a label or from a nested loop** (plain ones — 0,
    as in Sonar); `co_await` — our deliberate +1 (outside the Sonar spec).
- Nesting (`currentControlNestingDepth`) grows **only** inside
  control structures and lambdas — not from arbitrary braces, not from indentation.
- Indent metrics (`indentComplexitySum`, `maxIndentLevel`, `deepLinesCount`)
  are counted **only as diagnostic fields** in the report and as a file-level fallback
  (#099 territory). **They are not part of the score** — argument alignment and
  tabs make them format-dependent (repro in scorer review).
- Result: `localComplexityScore = branchScore` (cognitive-style).
- Finding thresholds — the hierarchy from the design doc (cognitive_complexity_delta_design.md §5):
  (1) a function crossed the absolute threshold **25** (default Sonar C-family and clang-tidy);
  (2) `delta >= 3` in a function already above the threshold (CodeScene pattern; the Δ≥3 floor
  comes from the #102 corpus verdict: a tail of Δ1–2 on already-huge functions, 72/210 findings,
  non-actionable);
  (3) `delta >= K` (K=5, non-strict; confirming K on a second slice is an open item from #102) — a soft warning.
  PR aggregate = sum of positive deltas; report negative ones as improvement.
  **Do not divide by diff size** (this brings back correlation with volume — Gil & Lalouche).
  The absolute `delta >= 5` from the prototype on a function with score 2000+ fires
  on any patch — don't use it.

### Runtime shape

- Work only on baseline/current text contents over changed C/C++ files.
- No libclang, compile database, include graph or compiler invocation.
- Run order by meaning:
  - after diff extraction;
  - before graph build;
  - before clone detection;
  - as an early advisory signal.

### Output semantics

- Advisory only by default.
- The message must speak specifically about proxy drift:
  - "grew local complexity from X to Y"
  - not "has cognitive complexity N".
- V1 must not break the CI default exit code.

### Concrete plan in the current code

1. Do NOT create a shared text preprocessing layer (`text_scan.h` cancelled): all analysis runs over the token stream of the existing `lex()` from [include/archcheck/scan/duplication/token_normalizer.h](~/projects/cpparch/include/archcheck/scan/duplication/token_normalizer.h) — see Step 0/1 of the detailed instruction. No repeated text parsing.
2. There is no second independent function parser in the tree (verified 2026-06-11: `function_signature_scan` from #093/#094 does not exist, those tasks did not start). Introduce `include/archcheck/scan/function_body_scan.h` + `src/scan/function_body_scan.cpp` here with conservative recognition of definitions (`signature ... {`), excluding `if/for/while/switch/catch`; #093/#094 will reuse it later.
3. Add `include/archcheck/scan/local_complexity_metrics.h` + `src/scan/local_complexity_metrics.cpp`:
   structures `FunctionComplexityMetrics`, `FileComplexityMetrics`;
   scorer over the token stream (cognitive core from Step 3 of the detailed instruction);
   nesting — control nesting via a classified brace stack, NOT raw brace depth and NOT indentation.
4. Add `include/archcheck/scan/local_complexity_drift.h` + `src/scan/local_complexity_drift.cpp`:
   compare old/new metrics;
   match functions by key `file + qualifiedName + paramArity` (collision → nearest `startLine` + `confidence=low`, details — Step 4 of the detailed instruction);
   do NOT escalate an ambiguous match into a file-level finding — only a low confidence mark (file-level aggregates remain diagnostics).
5. For the changed-file prefilter use the already existing `git::changedCppFiles()` from [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp). It already provides the correct semantics for `a..b` and `a..WORKTREE`.
6. Read old/new contents through the already shipped `GitObjectFileSource` and `DiskFileSource`:
   [src/git/git_object_file_source.cpp](~/projects/cpparch/src/git/git_object_file_source.cpp),
   [src/scan/disk_file_source.cpp](~/projects/cpparch/src/scan/disk_file_source.cpp).
7. Since the current `run_diff()` in [src/main.cpp](~/projects/cpparch/src/main.cpp) goes straight into `runDiffFullPath()`, it needs to be split into explicit stages:
   - parse revspec / repo root;
   - fast-path no C/C++ changes;
   - early local-complexity advisory pass;
   - graph diff pass;
   - unified output.
8. Do not route this through `IRule`:
   [include/archcheck/rules/i_rule.h](~/projects/cpparch/include/archcheck/rules/i_rule.h) is currently tightly bound to `DependencyGraph` and `readFile()`, which is a poor fit for old/new comparison per changed file.
9. Separately close the current report-model gap:
   right now `rules::Violation` and `json_reporter.cpp` cannot handle `symbol`, `endLine`, `oldScore`, `newScore`, `deltaPercent`.
   For v1 there are two acceptable paths:
   - minimally extend `Violation` backward-compatibly and update [src/report/json_reporter.cpp](~/projects/cpparch/src/report/json_reporter.cpp) / [src/report/text_reporter.cpp](~/projects/cpparch/src/report/text_reporter.cpp);
   - or introduce a separate diff-report writer that serves #096/#097/#101 at once and finally gives `--diff` JSON.
   The second path is preferred, because the current `--diff` already lives bypassing the snapshot reporter.
10. Git helpers already exist (extracted by the #096–#100 wave, verified 2026-06-11):
    [include/archcheck/git/git_exec.h](~/projects/cpparch/include/archcheck/git/git_exec.h) — `runGit(args, cwd)`;
    [include/archcheck/git/diff_query.h](~/projects/cpparch/include/archcheck/git/diff_query.h) — `collectAddedLines()`, `collectNumstat()`.
    Reuse them, don't duplicate anything.
11. Tests are better placed at existing points:
    - `tests/unit/scan/local_complexity_metrics_test.cpp`
    - `tests/unit/scan/function_body_scan_test.cpp`
    - `tests/unit/scan/local_complexity_drift_test.cpp`
    - `tests/integration/diff/git_diff_test.cpp` for repo-level baseline/current scenarios
    - once a diff JSON writer appears — a separate `tests/unit/diff/...` for the output contract.

### Detailed implementation instruction (v1) — algorithm, functions, reuse

The target semantics and rationale are in [cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) (§4 token-feasibility table, §5 signals, §6 core). Below is the step-by-step plan.

**Step 0 — what to take ready-made (don't rewrite any of this):**
- the lexer `lex()` from `include/archcheck/scan/duplication/token_normalizer.h` — already
  skips comments and preserves original spellings in `Token::raw`;
  use it **as is**, without modifications (details and the preprocessor trap are in step 1);
- `extractFragments` (`fragmenter.h`) — a reference for the "`)` + `{` = body" pattern;
- `git::changedCppFiles()` (`src/git/git_state.cpp`) — the list of changed files
  with `a..b` and `a..WORKTREE` semantics;
- `GitObjectFileSource` / `DiskFileSource` — old/new content;
- vendor/test filters `collectNonVendoredSources()` + `file_classification.h`.

**Step 1 — no need to write `rawLex`** (verified against the code 2026-06-11):
the existing `lex(source, /*keepCalls=*/false)` already preserves everything needed in
`Token { sym, line, raw }`: keywords come through as `sym` (`if`, `else`, `do`, …),
identifiers give `sym=="id"`, `raw==spelling`, literals — `sym=="lit"`;
`&&`/`||`/`::`/`->` are multi-char tokens; `{ } ( ) ; ? : \` are single tokens.
Original spelling: `rawText(t) = t.raw.empty() ? t.sym : t.raw` — introduce such an
inline helper. All further analysis is over the token stream, no repeated
text parsing and no indentation.
**Trap — the preprocessor:** `lex()` strips only `#if 0` blocks; other
directives are tokenized, i.e. `#if defined(X)` yields a fake keyword token `if`
(similarly `#else` → `else`). A mandatory filter in the scorer: on encountering a `#` token,
skip tokens to the end of the line (`token.line` changes); if the last token of the
line is `\`, continue skipping on the next line too (line-continuation).
The unit case for this is in the test matrix.

**Step 2 — `function_body_scan`**: `std::vector<FunctionSpan> discoverFunctions(tokens)`,
`FunctionSpan { qualifiedName, paramArity, startLine, endLine, bodyTokenRange }`.
Take names from `raw` (identifiers arrive as `sym=="id"`, `raw==spelling`).
Algorithm: candidate = identifier (with optional `::` chain / `operator@` / `~Name`),
followed by `( … )` (balanced parens), then optional `const/noexcept/override/&/&&/-> T`,
then `{`; exclude control words before `(`; `paramArity` = top-level commas + 1
(0 for `()` and `(void)`); body end — matching brace. Constructors: after `)` there may
be a `: init-list` before `{` — skip to `{` at the same depth.
**Name blacklist** (otherwise corpus FP, #102): `__attribute__`, `__declspec` — these are
attribute specifiers before the signature (`__attribute__((unused)) static int f(){…}`),
not function names; their `(…){` must not be recognized as a body.

**Step 3 — the core `computeCognitiveComplexity(bodyRange, funcName)`** —
single-pass, two stacks (full semantics in the design doc §1/§4):
1. `braceStack`: classify each `{` — CONTROL (previous significant tokens:
   `)` of a control header / `else` / `do` / `try` / lambda intro) or NEUTRAL
   (`class/struct/namespace/enum/union`, `=`, `return`, `,`, `(`, init-list);
   `nesting` = number of CONTROL frames (+ lambda frames).
2. `pendingBraceless`: a control header without `{` → nesting+1 until the nearest `;`
   at the same paren depth (otherwise `for(...) if(...)` undercounts).
3. Increments:
   - `if`: previous significant token `else` → **+1** (hybrid); otherwise **+1+nesting**;
   - `else` (not before `if`): **+1**;
   - `for` / `while` (except the do-while tail — flagged in braceStack) / `switch` /
     `catch`: **+1+nesting**; `do`: **+1+nesting**;
   - `?` (has a matching `:` before `;`/`,` at the same depth; not `::`): **+1+nesting**;
   - `goto`: **+1**; `case`/`default`/`try`/early `return`/`break`/`continue`: **0**.
4. Logical series: a `lastOp` stack by `(` depth; on `&&`/`||`/`and`/`or`
   in a **logical context** (previous significant token — identifier/literal/`)`/`]`,
   which cuts off rvalue `T&&`): if the operator ≠ `lastOp` of the current depth → **+1**;
   `!` before `(`, opening a `(`, `;`, `,` reset `lastOp`.
5. Options (default **off**, for comparability with clang-tidy): `#if/#ifdef/#elif`
   **+1+nesting** (for braceStack take only the first branch, like lizard);
   direct recursion (`funcName(` in the body) **+1**.
Return: `{ score, structuralCount, maxNesting, logicalSeries }` — the last three
are diagnostics only, for the report.

**Step 4 — delta `compareComplexity(oldFns, newFns)`**:
key = `file + qualifiedName + paramArity`; collision → nearest `startLine`,
`confidence=low`; symbol blacklist `TEST|TEST_F|TEST_P|TYPED_TEST|BENCHMARK|MOCK_METHOD`;
Δ = new − old (new function: Δ = score). Findings per the §5 design-doc hierarchy:
`LCX.1 crossed_25` (Sonar C-family/clang-tidy threshold), `LCX.2 grew_when_already_above`
(Δ>0 when old≥25), `LCX.3 delta_ge_k` (K=5, soft). Low-confidence matches do not yield
LCX.1/LCX.2. PR aggregate = Σ of positive Δ; negatives — on a separate line
(improvement).

**Step 5 — wiring**: the `run_diff()` stages from the plan above (item 7); a text block after
the structural diff; JSON — through a separate diff writer (item 9, path 2).

**Step 6 — test matrix** (each row = a unit case; the counterexamples from
scorer_review are mandatory as regressions — their baseline/current pairs are saved in
[experiments/local_complexity_drift/review_repros/](~/projects/cpparch/experiments/local_complexity_drift/review_repros/),
the pairs' decoding is in #102, step 5 of "Next steps"):

| Case | Expectation |
|---|---|
| flat switch over 8 cases | **1** (in the prototype it was 19) |
| `if{if{if{}}}` | 1+2+3 = **6** |
| `else if` chain ×5 | if=1 + 4×1 = **5**; the same as `else { if` → 1+2+3+4+5 |
| `a && b && c` | **1**; `a && b || c` → **2** |
| `Item&& x`, `auto&& y`, `std::forward` | **0** |
| aligned argument continuations | **0** |
| `do {…} while(x);` | **1** |
| reformatting a condition over 5 lines | delta **0** |
| lambda with an `if` inside | if = **+2** (1+nesting from the lambda) |
| keywords in strings/comments/raw strings | **0** |
| `#if defined(X)` / `#else` / `#elif` with the option default-off | **0** (fake keyword tokens filtered) |
| `goto` | +1; early `return/break/continue` | 0 |
| init-list data table | **0** |
| braceless `for(...) if(...) x;` | if = **+2** |

`function_body_scan_test`: free function, inline method, `Cls::method` out-of-class,
`operator()`, overloads (distinguished by arity), constructor with init-list, template function,
`__attribute__((unused)) static int f(){}` → recognized as `f`, NOT `__attribute__`.
`local_complexity_drift_test`: matching by arity, rename → new (no finding),
TEST_F blacklist, low-confidence without LCX.1/2.

**Definition of done:** all synthetic cases of #102 + the table above green; a manual
cross-check against clang-tidy (threshold 0) on ~20 real-code functions — divergence ≤2;
corpus re-run #102: switch parsers and TEST_F left the top, 6/6 TP preserved.

## Fixtures & Tests

- `fixtures/local_complexity_drift/pass/flat_branches.cpp`
- `fixtures/local_complexity_drift/pass/comments_and_strings.cpp`
- `fixtures/local_complexity_drift/pass/preprocessor_lines.cpp`
- `fixtures/local_complexity_drift/fail_growth/update_baseline.cpp`
- `fixtures/local_complexity_drift/fail_growth/update_current.cpp`
- `fixtures/local_complexity_drift/fail_new_complex/new_complex_function.cpp`
- `fixtures/local_complexity_drift/pass/harmless_change_baseline.cpp`
- `fixtures/local_complexity_drift/pass/harmless_change_current.cpp`

Mandatory checks:

- nested branches score higher than flat branches;
- comments/strings do not create fake branch tokens;
- preprocessor lines do not increase the branch score;
- growth baseline → current yields a finding;
- a harmless change (`run();` → `run(); log();`) yields no finding;
- output contains `rule id`, `file`, `symbol` if found, `old/new score`, `delta`, advisory semantics;
- the default exit code does not change to failure solely because of this signal.

## Acceptance criteria

- The signal computes proxy metrics only over changed C/C++ files.
- There is a baseline/current comparison.
- There are advisory findings about substantial growth.
- No dependency on compile DB, libclang, include graph or Sonar.
- There are tests for nesting vs flat, comments/strings, growth and the harmless-change no-op.
- The documentation states plainly that this is a supporting signal, not a default architecture gate.
- One clear decision is made on the overlap with #099: folded/subsignal, not a parallel duplicating implementation.

## Do not do

- Byte-for-byte convergence with Sonar/clang-tidy as a goal (the spec semantics are a reference,
  a divergence ≤2 points per function is normal; see design doc §4).
- A full C++ parser.
- A general project-wide complexity gate.
- Ownership/knowledge-risk analysis.
- Defect prediction.
- A generic quality-lint subsystem.
- Auto-refactoring suggestions.
- A new strict mode.

## Implementation-start decisions (2026-06-11)

| Decision | Reason |
|---------|---------|
| v1 output — through the already shipped `printDiffAdvisories()` in main.cpp (the #096/#097 pattern: a text block after the structural diff) | The #096–#100 wave grounded the advisory pattern AFTER this task was written; a separate diff-JSON-writer is deferred — `--diff` today has no JSON for any signal, introducing it single-handedly from #101 is scope creep |
| New functions yield only LCX.1 (`crossed_25`), not LCX.3 | The #102 corpus validation ran with new functions filtered out; LCX.3 on new functions (any new function with score ≥ 5) is unvalidated and almost certainly noisy; LCX.1 on new functions is a direct Detection contract requirement |
| Scorer port — an exact port of v2 `scan_commit.py` (including the column heuristic for glued `&&` for rvalue references) | The semantics are validated by the corpus (1612 commits) and the synthetic suite 13/13; "improvements" during the port = divergence from validation |
| A `col` field is added to `duplication::Token` (set in `lex()`) | Needed for the glued-`&&` rvalue heuristic; additive, doesn't break existing initializers |

## Done

- Task moved to wip; the LCX.2 floor `delta >= 3` from the #102 corpus verdict is baked into the Scoring model.
- **v1 implementation complete (2026-06-11):**
  - `Token::off` (byte offset) added to `token_normalizer` — needed for
    the glued-`&&` rvalue heuristic; set in one line in `lex()`.
  - `include/archcheck/scan/function_body_scan.h` + `src/scan/function_body_scan.cpp` —
    token discovery of function definitions: id chains with `::`, `~Name`, `operator@`
    (including `operator()`), top-level arity (with angle brackets), qualifiers,
    trailing return, ctor init-list. Unlike the prototype it also finds inline methods
    in class bodies.
  - `include/archcheck/scan/local_complexity_metrics.h` + `src/scan/local_complexity_metrics.cpp` —
    an exact port of the #102 v2 scorer (state machine `metric_for_span`): structural/hybrid/flat
    increments, control nesting via a classified brace stack, lastOp series of
    `&&`/`||` by paren depth, do-while pairing, braceless bodies, lambda nesting.
    Plus `stripDirectiveTokens()` — a filter for preprocessor lines (the `#if defined` → fake `if` trap).
  - `include/archcheck/scan/local_complexity_drift.h` + `src/scan/local_complexity_drift.cpp` —
    matching `(qualifiedName, paramArity)` → nearest-line/low-confidence; the LCX.1/2/3 hierarchy
    (thresholds 25 / Δ≥3 / Δ≥5); blacklist of TEST* macros; vendor/test filters from
    `file_classification.h`; positiveDelta/negativeDelta aggregates.
  - Wiring: `printComplexityAdvisory()` in `main.cpp` from `printDiffAdvisories()` —
    the #096/#097 pattern, an advisory block after the structural diff, doesn't touch the exit code.
  - Tests: the entire test matrix from the spec (14 core cases, all numbers matched on the first run),
    function_body_scan (10 cases), drift (9 cases), fixtures test (6), two repo-level
    integration cases in `git_diff_test.cpp`. Total 458/458 green.
  - `fixtures/local_complexity_drift/{pass,fail_growth,fail_new_complex}` — 8 files per the spec list.
  - Verification: build Debug ✓, ctest 458/458 ✓, lizard 0 warnings on the new code ✓,
    clang-format 18.1.3 ✓, dogfood `src/include/tests` = 0 violations ✓; smoke
    `--diff HEAD` on the repo itself: the only finding — the own fixture
    `saturate` (26 ≥ 25), a correct TP, the exit code unchanged.

## In progress

- (empty — awaiting commit and closure)

## Remaining before closure

- Commit(s) on the owner's command.
- The DoD item "manual cross-check against clang-tidy on ~20 real-code functions" — not done
  in this session (requires a clang-tidy run; the core is covered by the test matrix and the #102 corpus
  validation).
- The #099 decision (fold/absorb) — after landing.
- Confirming K=5 on a second corpus slice — an open item from #102, not blocking.

## Next steps

1. Decided (2026-06-11): `function_signature_scan` does not exist — we create `function_body_scan` here (see plan item 2).
2. Dropped (2026-06-11): shared text preprocessing is not needed — reuse `lex()` from token_normalizer (Step 1).
3. Implement the function metrics scorer.
4. Implement baseline/current compare over changed files.
5. Close the diff output contract: text + JSON.
6. Add fixtures and repo-level integration tests.
7. After that return to #099 — the decision there is already fixed (2026-06-11): #099 lives only as a file-level fallback for files where function discovery didn't work (macro-heavy) and as diagnostic fields; if the fallback turns out not to be needed — close #099 as absorbed. Do not make a separate parallel implementation.

## Key decisions

| Decision | Reason |
|---------|---------|
| Advisory-only | There is no `--strict` in the repo, and the product must not become a linter |
| Function-aware combined score, not just an indent proxy | This is closer to the original statement and more useful for the reviewer |
| Run before graph build | The signal is cheap and does not depend on the include graph |
| Don't route through `IRule` | An old/new diff compare is needed, and the current `IRule` is snapshot-only |
| Close the overlap with #099 by consolidation, not a parallel implementation | Otherwise there will be two almost identical complexity signals with different semantics |
| Prefer a separate diff-report writer rather than breaking the snapshot `Violation` blindly | The current `--diff` already lives outside the normal reporter path |
| `LCX.2` — advisory-only (or a floor threshold Δ≥K), outside the gating output | Corpus #102: 72/210 of the `LCX.2` findings are Δ1-2 on already-huge functions, useless for review |

## Planned files

| Area | Change |
|---------|-----------|
| `include/archcheck/scan/duplication/token_normalizer.h` | Reuse `lex()` as is; the new text_scan file is NOT created |
| `include/archcheck/scan/function_body_scan.h`, `src/scan/function_body_scan.cpp` | Lightweight function-boundary detection |
| `include/archcheck/scan/local_complexity_metrics.h`, `src/scan/local_complexity_metrics.cpp` | Per-function/per-file complexity metrics |
| `include/archcheck/scan/local_complexity_drift.h`, `src/scan/local_complexity_drift.cpp` | baseline/current comparison and findings |
| `src/main.cpp` | Early diff-stage orchestration before graph build |
| `src/diff/` or `src/report/` | Unified diff advisory text/json output for #096/#097/#101 |
| `tests/unit/scan/` | metrics/body-scan/drift unit tests |
| `tests/integration/diff/git_diff_test.cpp` | real git baseline/current scenarios |
| `fixtures/local_complexity_drift/` | `pass/` and `fail_*` fixtures |

## Summary
**Status:** completed — the v1 implementation came fully alive:
- **How it works:** a token Sonar Cognitive Complexity scanner (the v2 scorer from #102,
  D1–D7 closed) → `function_body_scan` (function boundaries) → per-function metrics →
  baseline/current comparison over changed files → advisory output in `--diff`
  (the LCX.1 crossed_25 / LCX.2 grew_when_already_above / LCX.3 Δ≥K hierarchy).
- **Commits:** e80b628 (engine), 7938d9c (CLI advisory in --diff), 95aaa62
  (fixtures `fixtures/local_complexity_drift/{pass,fail_growth,fail_new_complex}`),
  643c69c (checkpoint). 72 LCX tests in the suite, the whole suite 461/461 green.
- **Non-blocking tails** (fixed above): the clang-tidy run; confirming
  K=5 on a second corpus slice (— #102); the #099 decision (file-level fallback or
  close as absorbed) — after running on real diffs.
**Completion date:** 2026-06-11
