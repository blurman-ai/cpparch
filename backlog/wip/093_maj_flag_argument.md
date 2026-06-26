# [CHEAP-DRIFT][SCAN] Flag-Argument Heuristics

**Created:** 2026-06-10
**Started:** 2026-06-23
**Status:** wip — `ARG.1.flag_argument_signature` (signatures) shipped (c0f37db, in release 0.1.0); remainder = `ARG.2` (call sites: ≥2 `true`/`false` literals in a single call)
**Module:** SCAN / DIFF / REPORT
**Priority:** major
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #030 (baseline_file_flag), #051 (config_loader_v1), #075 (trusted_diff_workflow), #086 (drift2_cycle_default_gate), #089 (boolean_state_drift), #090 (bool_field_accretion — sample delta-first advisory on the fast backend)

## Goal

Add a cheap heuristic for detecting interface drift:

- signatures with boolean flags;
- calls with multiple `true` / `false` literals.

This is needed precisely as an early-warning signal "behavior is leaking into the interface", not as a strict semantic rule.

## Context

- Source of the task: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 1.
- Boolean **fields** of structs are already covered by the shipped rule `DRIFT.BOOL_FIELD_ACCRETION` (#090, in 0.1.0). This task is about boolean **flag arguments of functions**: an orthogonal cheap-drift signal, not an "extension" of the bool-field rule. Shared with #090 is only the methodology (delta-first advisory, token-based fast backend), not the phenomenon.
- The current `archcheck` runtime is built around a graph-backed `IRule` and a minimal `Violation { ruleId, file, line, message }`. The task must not break the current `--format json` v1 and must not drag in a second parallel report format.
- The current config contract (`version / modules / rules / thresholds`) does not allow an arbitrary top-level `cheap_drift:`. The first implementation must live on built-in defaults; a separate schema extension is a separate task.
- By product positioning this is **not an intrinsic default rule at the SF/Lakos level**, but a cheap drift heuristic. That means: advisory-first, delta-first, no zero-config gate over the whole legacy tree.

## Execution plan

### Detection contract

- Declaration/definition:
  - finding at `>= 2` parameters of type `bool`;
  - finding at `1` `bool` parameter with a suspicious name (`enable*`, `disable*`, `use*`, `force*`, `skip*`, `with*`, `without*`, `no*`, etc.).
- Call site:
  - finding at `>= 2` `true` / `false` literals in a single call;
  - finding at adjacent boolean literals;
  - a mix of boolean literals with magic numbers is admissible as an additional signal only if it doesn't worsen recall/precision on fixtures.

### Detector algorithm (token-based)

Work over the token stream of `rawLex()` (see #101 step 1 — the lexer already skips
comments/strings/raw strings), not over text.

**Signatures (`ARG.1`)** — on top of `discoverFunctions()` from the shared
`function_signature_scan` (#101 step 2 describes the same boundary detector):
1. For each `FunctionSpan` parse the parameter list by top-level commas
   (depth of `()`/`<>`/`[]` = 0).
2. A parameter is considered a bool parameter if its tokens contain `bool`
   (including `const bool`, `bool*`/`bool&` do NOT count — a pointer/reference is an
   out-parameter, a different pattern).
3. `boolParamCount >= 2` → finding; `== 1` and the parameter name matches
   `^(enable|disable|use|force|skip|with|without|no|is|allow)` (case-insensitive,
   snake/camel) → finding. Name = the last identifier of the parameter.
4. Declarations without a body (`;` instead of `{`) also count — the flag argument is visible
   already in the header; dedup by `qualifiedName+arity` (declaration vs definition —
   one finding).

**Call sites (`ARG.2`)**:
1. A call candidate = identifier + `(` (the previous significant token NOT in
   {`if`,`for`,`while`,`switch`,`catch`,`return` — return is admissible, but it is a call});
   exclude control words by the Noise control list.
2. Inside the parens up to the matching `)` count top-level `true`/`false` literals.
3. `>= 2` literals OR two literals in a row separated by a comma → finding on the call line.
4. Don't look into nested calls separately — each `ident(` is handled by
   its own pass (stack).

**Delta mode**: after a full scan of the file keep findings whose lines fall
into the diff's added lines (`collectAddedLines()` from #096) — a filter over the result, not
a separate scanner.

### Runtime shape

- Implement as a light text/token pass, without libclang.
- If a shared helper for the cheap token scan is needed along the way, it must be minimal and immediately reusable in #094 and #095. Don't build a "universal parser framework".
- Comments and string literals are removed before analysis, but with line mapping preserved.
- Integration with diff is done on top of the existing git/diff layer, not via scattered shell calls from different places.

### Noise control

- Explicitly exclude `if`, `for`, `while`, `switch`, `catch`.
- Function-pointer syntax, macro-heavy cases, and template edge cases are not required to be supported in the first pass, if it sharply inflates scope.
- Full scan — advisory/report only.
- Gate candidates — only new/changed lines.

### Reporting

- Base rule ids:
  - `ARG.1.flag_argument_signature`
  - `ARG.2.boolean_literal_call`
- If the current `Violation` is enough, use it without extension.
- If an extension is still needed, make it only backward-compatible and immediately useful for at least one more cheap-drift task.

### Concrete plan in the current code

1. Don't invent a new file source: for the full scan take `scan::FileSource` and `collectNonVendoredSources()` from [src/scan/project_files.cpp](~/projects/cpparch/src/scan/project_files.cpp), because vendor/test exclusions are already centralized there.
2. Extract text-preprocess primitives from [src/scan/include_scanner.cpp](~/projects/cpparch/src/scan/include_scanner.cpp) into a new shared helper `include/archcheck/scan/text_scan.h` + `src/scan/text_scan.cpp`:
   comment stripping, raw-string blanking, line map, identifier helpers. Otherwise #093/#094/#095/#099 will copy the same logic.
3. Add `include/archcheck/scan/function_signature_scan.h` + `src/scan/function_signature_scan.cpp` with an approximate `SignatureInfo`:
   `name`, `owner`, `line`, `paramCount`, `boolParamCount`, `boolParamNames`.
   This layer will then be immediately reused by #094.
4. On top of it add the detector `src/scan/flag_argument_scan.cpp`, which:
   separately walks signatures;
   separately looks for call sites by lines/parens in the already-cleaned text;
   immediately returns `rules::ViolationList`.
5. Do the full-scan wiring in [src/main.cpp](~/projects/cpparch/src/main.cpp) inside `run_check()`:
   after creating `DiskFileSource src(root)` and before `applyBaselineAndReport()`.
   It doesn't need to be threaded through `IRule`: the current `IRule` is rigidly graph-backed.
6. Do the delta wiring not through `RegressionReport`, but next to `run_diff()`:
   take the file prefilter from `git::changedCppFiles()` in [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp),
   take line-level gating from the future shared helper `git/diff_query.cpp` (the same helper is needed by #096).
7. Lay out the test matrix as follows:
   low-level text preprocessing — new `tests/unit/scan/text_scan_test.cpp`;
   signature extraction — new `tests/unit/scan/function_signature_scan_test.cpp`;
   rule behavior — new `tests/unit/scan/flag_argument_scan_test.cpp`;
   repo-level diff scenarios — extend [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/flag_argument/pass/basic.cpp`
- `fixtures/flag_argument/pass/control_flow_not_call.cpp`
- `fixtures/flag_argument/fail_signature/two_bool_params.cpp`
- `fixtures/flag_argument/fail_signature/suspicious_single_bool.cpp`
- `fixtures/flag_argument/fail_call/multiple_bool_literals.cpp`
- `fixtures/flag_argument/fail_call/mixed_literals_and_magic.cpp`

Required checks:

- full scan finds signature findings;
- full scan finds call-site findings;
- `if (ok && bad) {}` is not parsed as a call;
- diff mode reports only added/changed-line findings;
- baseline/legacy suppression does not turn the rule into a full-tree gate;
- JSON/text output contains stable `ruleId`, `file`, `line`, `message`.

## Acceptance criteria

- Without libclang and without compile-commands.
- Without a new top-level `cheap_drift:` in `.archcheck.yml`.
- Without a second, incompatible JSON format.
- The FP/FN profile is described at least on fixtures and one dogfood run.
- By default the rule does not fail CI on a legacy full scan.

## Don't do

- A full C++ parser.
- Macro expansion.
- Cross-file symbol resolution.
- A separate generic "rule engine" for cheap drift.

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Fix the minimal token-scan contract and line mapping.
2. Implement the detector for signatures and the detector for call sites.
3. Wire up diff filtering.
4. Run fixtures and dogfood on `archcheck` itself.
5. Decide whether an upgrade of `Violation` is needed at all, or the current contract is enough.

## Key decisions

| Decision | Reason |
|---------|---------|
| Advisory-first, delta-first | Otherwise check quickly slides into a false-positive "linter over legacy" |
| Without `cheap_drift:` in the current task | The current config schema doesn't allow it |
| A shared scanner helper is allowed, a shared framework is not | Reuse for #094/#095 is needed, but without a YAGNI abstraction |
| Call-site and signature signal in one task | This is one and the same interface drift family |

## Planned files

| Area | Change |
|---------|-----------|
| `src/scan/` or `src/cheap_drift/` | Light token/text scanner helper |
| `src/main.cpp` and/or a separate orchestration layer | Wiring the pass into full/diff modes |
| `src/report/` | Only if a backward-compatible reporting upgrade is needed |
| `tests/unit/` | Tests of scanner/detector logic |
| `tests/integration/` | Diff-mode scenarios |
| `fixtures/flag_argument/` | `pass/` and `fail_*` sets |
