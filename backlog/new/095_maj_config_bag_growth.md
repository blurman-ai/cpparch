# [CHEAP-DRIFT][SCAN] Config-Bag Growth

**Created:** 2026-06-10
**Started:** —
**Status:** new
**Module:** SCAN / DIFF / REPORT
**Priority:** major
**Complexity:** medium
**Blocks:** —
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #030 (baseline_file_flag), #051 (config_loader_v1), #075 (trusted_diff_workflow), #089 (boolean_state_drift), #090 (bool_field_accretion — sample delta-first advisory), #093 (flag_argument), #094 (param_count_and_accretion)

## Goal

Find structs/classes that gradually turn into a "bag of options":

- too many fields in `*Config` / `*Options` / `*Settings` and neighboring types;
- a noticeable growth of fields relative to the base revision.

This is a cheap heuristic about configuration and implicit-state accumulation, not a general class-metrics tool.

## Context

- Source of the statement: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 3.
- The statement logically continues the cheap-drift family from #093/#094: first flags and signatures, then data carriers into which everything gets stuffed.
- The current shipped runtime has no production rule about boolean-field growth; there's only research around boolean-state drift (#089). So this is not about "extending the current shipped check" but a new cheap advisory signal.
- To avoid sliding into a generic "we measure every class in a row", the first pass should work only on candidate names (`*Config`, `*Options`, `*Settings`, `*Params`, `*Context`, etc.).
- History growth via `git log` is mentioned in the original statement as an option, but in the current product it's more useful to first close out full scan + diff/ref comparison. The historical window is better deferred only if a clear need remains after v1.

## Execution plan

### Detection contract

- `CFG.1.config_bag_size`:
  - a finding if a candidate type exceeds the field-count threshold;
  - the full snapshot remains advisory.
- `CFG.2.config_bag_accretion`:
  - a finding if a candidate type grew noticeably in field count relative to the base ref;
  - a gate candidate is permissible only for new fields in a changed type and only after a fixture noise-eval.

### Scanner shape

- Find `struct|class <Name> { ... };` approximately, without an AST: token `struct`/`class` +
  identifier + (opt. `final`, `: bases` up to `{`) + matching brace. Nested types —
  separate `TypeInfo` (name `Outer::Inner`). Forward declarations (`;` instead of `{`) — skip.
- Count likely fields (by tokens inside the body, depth of nested `{}` = 0):
  - a statement up to `;` that has no `(` before `;` (cuts off methods/constructors),
    no `using`/`typedef`/`friend`/`static_assert`/`template`;
  - `static` fields are not counted (not instance state);
  - multiple declarators via a comma (`bool a, b;`) = number of commas + 1;
  - bitfields (`int x : 3;`) — an ordinary field;
  - access labels `public:`/`private:`/`protected:` — skip tokens.
- Candidate names: suffixes `Config|Options|Settings|Params|Parameters|Opts`
  (case-insensitive on the last word of the name). `*Context`/`*State` — outside the default.
- v1 thresholds: `CFG.1` — `fieldCount > 15` (the Hadoop case, Xu FSE 2015 — growth 17→173;
  fewer than 15 is all over the place in legitimate configs); `CFG.2` — `+3` fields in one
  diff OR `+5` relative to the base ref without a single removal. Calibrate on the corpus
  (our eye-check: config-bag ≈ 40% boolean candidates — the threshold must cut FP).
- Match old/new by `file + qualified type name` (renaming a type = new, no finding).

### Scope discipline

- By default, don't analyze every type in a row.
- Don't include `*State` in the default candidate list until there's confirmation that it doesn't break accuracy.
- If a field is declared across multiple lines or several fields go in one declaration, an approximate count is allowed, but the behavior must be pinned down by tests.
- A separate config surface for the suffix list and thresholds — not in this task, if it requires extending the schema.

### Concrete plan in the current code

1. Implement a separate lightweight type scanner `include/archcheck/scan/type_body_scan.h` + `src/scan/type_body_scan.cpp`, rather than trying to stretch this onto `function_signature_scan`.
2. Inside it, reuse the shared `text_scan.cpp` from #093:
   we need already-cleaned text with strings preserved, otherwise access labels, multiline declarations and comments quickly break the field counter.
3. Return `TypeInfo { name, kind, startLine, endLine, fieldCount, isCandidateName }`.
   Keep the candidate suffix list as a `constexpr std::array` inside the scanner, not in the current config loader.
4. Do the full scan via `collectNonVendoredSources()` and `DiskFileSource`, to immediately inherit the same exclusions already applied in graph/duplication.
5. Count `CFG.2.config_bag_accretion` only on changed files:
   the list of paths from `git::changedCppFiles()`;
   old/new contents via `GitObjectFileSource` and `DiskFileSource`;
   match by `file + type name`.
6. Don't touch [src/config/config_loader.cpp](~/projects/cpparch/src/config/config_loader.cpp):
   the current schema only knows `version/modules/rules/thresholds`, and this task should not extend the schema for the suffix list.
7. Lay out the tests like this:
   a new `tests/unit/scan/type_body_scan_test.cpp` for candidate names, field counting and access labels;
   if needed 1-2 end-to-end compare scenarios in [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/config_bag_growth/pass/non_candidate_struct.cpp`
- `fixtures/config_bag_growth/pass/small_options.cpp`
- `fixtures/config_bag_growth/fail_size/render_options.cpp`
- `fixtures/config_bag_growth/fail_accretion/network_config_baseline.cpp`
- `fixtures/config_bag_growth/fail_accretion/network_config_current.cpp`
- `fixtures/config_bag_growth/pass/methods_not_counted.cpp`
- `fixtures/config_bag_growth/pass/access_labels.cpp`

Mandatory checks:

- the candidate-name filter works;
- non-candidate types are ignored by default;
- methods are not counted as fields;
- access labels don't break the count;
- field growth between old/new is detected;
- the signal stays advisory-first and doesn't punish the legacy full tree.

## Definition of done

- No AST / libclang.
- No new top-level config block.
- The historical `git log` mode is not required for v1.
- The candidate suffix list for the first pass is pinned down.
- There are `pass/` and `fail_*` fixtures, as the backlog policy requires for rules.

## Do not do

- A universal "class size" detector over any names.
- Semantic classification of "which field is really config/state".
- Git-history growth within this same task, if it gets in the way of closing the diff/full core.
- A separate ML/score system for "how bad the type is".

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Pin down the candidate suffix list for the first pass.
2. Implement the `class/struct` range scanner.
3. Implement the field counter and old/new comparison.
4. Cover fixtures for access labels, methods and multiline cases.
5. Check the signal on dogfood and 1-2 external OSS projects.

## Key decisions

| Decision | Rationale |
|---------|-----------|
| Analyze only `*Config`-like types | Otherwise the rule quickly becomes a noisy general metric |
| Move history-growth out of the core v1 | First we need a useful and cheap full/diff path |
| Don't include `*State` by default | Too high a risk of false positives on legitimate state-carrier types |
| Field counting may be approximate, but testable | The goal is a cheap drift signal, not an exact parser |

## Planned files

| Area | Change |
|---------|--------|
| `src/scan/` or `src/cheap_drift/` | Class/struct scanner and field counter |
| `src/main.cpp` / orchestration | Full scan and comparison against the base ref |
| `tests/unit/` | Field counting and candidate-name filtering |
| `tests/integration/` | Diff/accretion scenarios |
| `fixtures/config_bag_growth/` | `pass/` and `fail_*` fixtures |
