# [CHEAP-DRIFT][SCAN] Parameter Count and Accretion

**Date created:** 2026-06-10
**Date started:** —
**Status:** new
**Module:** SCAN / DIFF / REPORT
**Priority:** major
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #030 (baseline_file_flag), #051 (config_loader_v1), #075 (trusted_diff_workflow), #089 (boolean_state_drift), #090 (bool_field_accretion — a delta-first advisory exemplar), #093 (flag_argument)

## Goal

Catch two cheap signals of interface degradation:

- overly wide signatures;
- growth in the number of parameters of an already-existing function.

This is a proxy for "instead of a new type/model we keep appending arguments".

## Context

- Source of the assignment: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 2.
- Inside the repo itself there is already a hard limit on the argument count via `lizard` in dogfood/static-analysis, but this does **not** mean the product needs yet another global lint-style gate. Here we are talking about a cheap drift heuristic for user repositories, and above all about delta/accretion.
- Absolute parameter count without delta context quickly turns into "yet another style checker", which conflicts with archcheck's positioning. So a gate on the full snapshot is excluded by default.
- For the accretion signal the current violation baseline is insufficient as the sole mechanism: we need to see "was N, became M". So the first practically useful mode is a comparison with a git ref / diff, not an attempt to forcibly cram everything into the existing baseline violations list.

## Execution plan

### Detection contract

- `ARG.3.long_parameter_list`:
  - finding on new/changed signatures with a parameter count above the threshold;
  - default threshold roughly `> 4`, but the check stays advisory-first.
- `ARG.4.parameter_accretion`:
  - finding if an existing function gained `>= 2` parameters relative to the base revision;
  - for a gate candidate, what matters is precisely growth on a changed symbol, not any "long-bad" long list.

### Matching strategy

- Reuse the token scan substrate from #093, if it already exists.
- For each likely function declaration/definition, assemble an approximate key:
  - path;
  - function name;
  - enclosing class/namespace, if cheaply extractable;
  - line anchor.
- If an exact correspondence between old/new is not found, an ambiguous match is downgraded to advisory or skipped. One must not gate on a fragile match.

### Algorithm

1. `SignatureInfo` collects `paramCount` with the same top-level-comma parsing that
   ARG.1 uses in #093 (one parser function for both rules): depth of `()`/`<>`/`[]` = 0;
   `()` and `(void)` → 0; `...` (variadic) counts as one parameter.
2. `ARG.3.long_parameter_list`: full scan → findings on `paramCount > 4`
   (Core Guidelines I.23 "fewer than four"); advisory. In diff mode — a filter
   by added lines (as in #093), this is a gate candidate.
3. `ARG.4.parameter_accretion`: old/new `SignatureInfo` over changed files
   (`GitObjectFileSource` vs `DiskFileSource`); matching by the key
   `file + qualifiedName` with a **fallback by decreasing strictness**:
   exact (old name+arity == new → no delta, skip) → name matched,
   arity grew (accretion candidate) → several same-name: nearest startLine,
   confidence=low. Finding when `newArity - oldArity >= 2` and confidence != low.
4. Overloads: if old has exactly one function named N and new has exactly one
   named N — a confident match even with a different arity (this is accretion);
   if there are several candidates on both sides — all pairs low → skip.
5. Matching test cases: adding an overload (NOT accretion — old 1, new 2,
   a matching arity pair exists → skip); growth 3→5 (finding); rename (skip).

### Scope discipline

- Do not count `void` as a parameter.
- Account for default arguments approximately, but do not build a full declarator parser.
- Function pointers, lambdas in parameters and macro-generated signatures may be partially ignored if this sharply simplifies the implementation and is explicitly documented.
- Aim the first pass at `--diff` / base-ref comparison. A separate saved snapshot for symbol metadata — only if without it a useful signal is impossible.

### Concrete plan in the current code

1. Do not make a second scanner: extend the shared `function_signature_scan` from #093, rather than writing yet another parser for `param_count`.
2. In `SignatureInfo` immediately provide the fields needed specifically for this task:
   `qualifiedName`/`owner`, `line`, `paramCount`, `hasVoidParameterList`, `hasDefaultArgs`.
3. Compute `ARG.3.long_parameter_list` on a full scan via `collectNonVendoredSources()` and the current `DiskFileSource`, without a graph path and without `IRule`.
4. Build `ARG.4.parameter_accretion` by comparing two snapshots over changed files:
   take the file list from `git::changedCppFiles()`;
   read the baseline side via `GitObjectFileSource` from [src/git/git_object_file_source.cpp](~/projects/cpparch/src/git/git_object_file_source.cpp);
   read the current side via `GitObjectFileSource` or `DiskFileSource` depending on `WORKTREE`.
5. Do not put matching into baseline JSON v1:
   in the current code the baseline stores only `(ruleId, file, line)`, which is insufficient for symbol accretion.
   Do the matching locally in the diff path: `file + function name + nearest old line`.
6. If a single file has several same-name overloads and the match is ambiguous, do not gate:
   either an advisory finding, or a skip. This must be fixed directly in the implementation.
7. Tie the tests to already-existing entry points:
   parsing/counting — new `tests/unit/scan/function_signature_scan_test.cpp`;
   accretion matching — new `tests/unit/scan/parameter_accretion_test.cpp`;
   git/ref scenarios — extension of [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/param_count/pass/four_params.cpp`
- `fixtures/param_count/fail_long/five_params.cpp`
- `fixtures/param_count/fail_long/member_function.cpp`
- `fixtures/param_count/pass/void_param.cpp`
- `fixtures/param_count/pass/default_args.cpp`
- `fixtures/param_count/fail_accretion/baseline.cpp`
- `fixtures/param_count/fail_accretion/current.cpp`

Mandatory checks:

- the long parameter list is detected stably;
- `void` is not counted as a separate argument;
- parameter growth between old/new is recorded;
- ambiguous matching does not become a gate;
- JSON/text output does not break the existing contract;
- the dogfood run does not produce an explosion of false positives on the current `archcheck` code.

## Readiness criteria

- Absolute count stays advisory by default.
- The value check is in delta/accretion, not in punishing the legacy whole tree.
- No libclang.
- No new config top-level key.
- The trade-off between matching precision and implementation simplicity is described.

## Do not do

- AST-level identity matching of functions.
- A full type pretty-printer.
- Historical analytics over `git log` within this task.
- A universal symbol database "for the future".

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Reuse or extract a minimal function-signature scanner.
2. Fix the old/new matching strategy.
3. Implement the long-count and accretion detectors.
4. Run the diff fixtures and dogfood.
5. Decide whether a separate symbol snapshot is needed, or whether comparison with a git ref is enough.

## Key decisions

| Decision | Reason |
|---------|---------|
| The task centers on `parameter accretion`, not just `max_params` | It is closer to the drift model and conflicts less with "not a linter" |
| An ambiguous match must not gate | One cannot build a CI fail on a fragile heuristic match |
| Reuse the scanner from #093, if it already exists | Minimum duplication without a new abstraction |
| Saved baseline metadata is not mandatory in v1 | The project already has a useful `--diff`, and there is no baseline schema for symbol metrics yet |

## Planned files

| Area | Change |
|---------|-----------|
| `src/scan/` or `src/cheap_drift/` | Reusable signature scanner |
| `src/main.cpp` / orchestration | Comparison of current vs base ref |
| `tests/unit/` | Parameter counter and matching logic |
| `tests/integration/` | Diff/accretion scenarios |
| `fixtures/param_count/` | `pass/`, `fail_long/`, `fail_accretion/` |
