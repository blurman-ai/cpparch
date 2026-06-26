# [CORE][DOCS][BACKLOG] Consolidate the tech debt around contracts, CLI orchestration, and document drift

**Created:** 2026-06-02
**Started:** 2026-06-11
**Completed:** 2026-06-12
**Status:** completed
**Module:** CORE / CLI / DOCS / BACKLOG
**Priority:** major
**Difficulty:** L
**Blocks:** —
**Blocked by:** —
**Related:** #045 (docs_sync_roadmap_mvp_spec), #051 (config_loader_v1), #057 (lakos_fanout_coupling_checks), #060 (checker_validation_hardening_loop), #070 (checker_fp_fix_proposals)
**Source of truth:** [docs/architecture-spec.md](../../docs/architecture-spec.md), [docs/ROADMAP.md](../../docs/ROADMAP.md), [docs/config_format.md](../../docs/config_format.md)

## Goal

Gather into one task the accumulated **non-random tech debt** around shipped v0.1:
not new features, but the divergences between the code, the CLI contracts, the documents, and the backlog.
The task is needed as an umbrella: to first align the product reality, and only
then safely extend the rules and research branches.

## Context

The `archcheck` code core is already alive: there's `scan -> graph -> rules -> report`,
diff/drift, baseline, config loader, unit/integration tests. But around the core a
layer of debt has accumulated, of two kinds:

1. **Product contracts diverge from the actual implementation.**
2. **The documents and the backlog diverge from each other and from the current tree.**

This is no longer "cosmetics in the docs". Some of the divergences directly mislead the
user (`--config`, the diagnostics format, different thresholds in `check` and `--diff`).

## TL;DR

The main debt is not in the graph core, but in **alignment**:

- The CLI promises more than it delivers.
- The data model no longer covers the promised output contract.
- `main.cpp` has grown into an application god-file and become a source of mode divergence.
- The roadmap/backlog/research documents describe different states of the product.
- Some `wip` tasks reference already-deleted artifacts.

Until this is consolidated, any new feature raises the maintenance cost disproportionately.

## Tech debt inventory

### 1. False contract `--config`

- CLI help promises `--config <path> ... run check`.
- The loader actually parses `modules` + `layers` / `independence` / `forbidden`.
- But the runtime uses only `thresholds` from `Config`; the module rules are
  not applied at all.

Consequence:

- the user gets "config accepted", but the architecture rules from the config don't work;
- we have shipped surface area without shipped semantics;
- this is more dangerous than an ordinary TODO, because the behavior looks complete.

## What to do

- [ ] Make a product decision: either `--config` is temporarily validate-only, or
      bring config rules through to the runtime.
- [ ] If validate-only: honestly change help/README/spec and the exit behavior.
- [ ] If runtime: a separate task for `config -> rule pipeline`, without masquerading as a "minor fix".

### 2. The diagnostic contract diverges from the `Violation` model

- The documents and the AGENTS framing promise `file:line:column`.
- `Violation` stores only `file`, `line`, `message`, `ruleId`.
- The text/JSON reporters don't emit the column, because there's simply nowhere to get it.

Consequence:

- the user contract is already wider than the actual domain model;
- it will be painful later to add precise location-based rules and suppression.

## What to do

- [ ] Adopt one contract: either actually extend `Violation` to `column`,
      or downgrade all documents to `file[:line]`.
- [ ] If adding `column`: update the baseline/json schema and immediately fix a migration policy.

### 3. Different thresholds for `check` and `--diff`

- The ordinary `Lakos.GodHeader` lives at threshold `50`.
- The diff/report layer keeps its own default `30`.
- These are different sources of truth for one and the same entity.

Consequence:

- the same project can be green in `check` and red in `--diff`;
- the metrics and regression semantics become untrustworthy.

## What to do

- [ ] Bring the threshold defaults to one place.
- [ ] Pass them into diff-mode explicitly, not through hidden literals.
- [ ] Cover with a test exactly the consistency between `check` and `--diff`.

### 4. `src/main.cpp` has become an application god-file

- Right now there are ~570 lines of orchestration: parsing the CLI, baseline I/O, graph preview,
  diff-mode, drift-mode, config discovery, report dispatch.
- This is exactly where the mode divergences and duplicated defaults-selection rules already live.

Consequence:

- adding new modes gets ever more expensive;
- it's easy to get yet another "only this mode forgot to pass X" behavior;
- the application layer is no longer obvious.

## What to do

- [ ] Split the CLI orchestration into small application-level commands:
      `check`, `diff`, `graph`, `scan`, baseline helpers.
- [ ] Remove from `main.cpp` the policy decisions that should live in a shared layer.
- [ ] Leave in `main.cpp` only a thin dispatch.

### 5. SF.8 is checked more weakly than declared

- The current heuristic considers a header guarded if it saw any `#ifndef` in the first lines.
- There's no check for a pair of `#ifndef` + `#define` for the same guard.
- This creates false negatives and makes the rule less strict than its name suggests.

## What to do

- [ ] Strengthen SF.8 to a real include-guard pattern.
- [ ] Don't smear the logic: reuse existing scanner knowledge where possible.
- [ ] Add fixtures for a false `#ifndef` without guard semantics.

### 6. Config discovery depends on `cwd`, not on the checked root

- `archcheck /path/to/project` looks for `.archcheck.yml` from the process's current directory.
- This makes the behavior non-obvious for CI, monorepos, and external invocation.

## What to do

- [ ] Tie discovery to the `root` that is actually being checked.
- [ ] Explicitly document the precedence: `--config` > discovered-near-root > embedded defaults.

### 7. The documents describe several different products

- `docs/MVP.md` is still about the old pre-#006 MVP with `compile_commands.json` and dependency rules as the core.
- `docs/ROADMAP.md` simultaneously drags the duplication line-pass `#053` into the current scope.
- `docs/architecture-spec.md` in several places still uses stale vocabulary
  (`forbidden_deps` / `allowed_deps`) next to the already-adopted typed config contract.
- `AGENTS.md` is critically outdated: it says `pre-implementation`, "no src/tests/CMake", old naming rules.

Consequence:

- an agent or a new developer can't quickly understand what's actually shipped;
- on a document conflict, we have no stable footing;
- research decisions masquerade as product reality.

## What to do

- [ ] Close #045 as the nearest docs blocker.
- [ ] Update `AGENTS.md` to the actual state of the repo and the current style contract.
- [ ] Remove from the public documents wording that promises v0.2 features as v0.1-core.

### 8. The backlog contains stale WIP and broken links to deleted artifacts

- `docs/duplication_architecture.md` records that line-based `#053` and its tree are deleted.
- Meanwhile `backlog/wip/053_...` is still active and references the missing `experiments/line_duplication/*`.
- The roadmap continues to count `#053` as part of the active v0.1 narrative.

Consequence:

- the backlog loses value as a state system;
- old research looks "in progress" though it has already been cancelled by an architectural decision.

## What to do

- [ ] Move the cancelled branches out of `wip` into a correct state: close, archive, or rewrite as a historical note.
- [ ] Clean up the links to deleted artifacts.
- [ ] Reduce the duplication narrative to one live path: `#056` + `#070` + `#060`.

### 9. The research layer has grown larger than the product layer

- `src/include/tests` has fewer than a hundred files.
- `experiments/` already has well over a thousand files.
- This is normal for the research phase, but already calls for a separate hygiene policy.

Consequence:

- the signal-to-noise ratio when navigating the repo drops;
- the backlog and docs start referencing experimental artifacts as product truth;
- stale research artifacts live longer without a decision.

## What to do

- [ ] Lock in a rule: what counts as the product source of truth, and what is experiment-only.
- [ ] Limit direct references from the roadmap/current docs to experimental files without an ADR/summary layer.
- [ ] If necessary, move large research into subdirectories with an explicit lifecycle status.

### 10. Minor but systemic debt around the single source of truth

- literal thresholds and policy live in several places;
- docs/code_style and AGENTS diverge on naming (`name_` vs `_name`, `I*` vs no-`I`);
- some completed/new tasks already record one decision while the spec records another.

Consequence:

- every next change-set requires manual syncing;
- the probability of a silent re-divergence is high.

## What to do

- [ ] For every shipped user-facing contract, have exactly one product source of truth.
- [ ] Explicitly separate: spec / roadmap / historical backlog notes / experiment reports.

## Execution plan

### P0 — remove the untrustworthy contracts

- [x] Decision on `--config`: validate-only vs real runtime semantics. *(closed in #082 Slice 1/2: validate-only)*
- [x] Bring the diagnostic contract (`line`/`column`) in line with reality. *(closed in #082: downgraded to `file:line` everywhere)*
- [x] Bring the `GodHeader` thresholds between `check` and `--diff` together. *(2026-06-11: diff takes the threshold from the discovered config; the `MetricThresholds` default 30 → 50; a test-pin on default equality)*
- [x] Tie config discovery to `root`, not to `cwd`. *(2026-06-11: `discoverConfig(root)`, walkup from the analyzed root; the precedence is documented in config_format.md)*

### P1 — stabilize the application layer

- [x] Split `src/main.cpp`. *(2026-06-12: 786 → 275 lines; thin dispatch + `src/cli/` units)*
- [x] Pull out the baseline/diff/config helpers into separate application units. *(2026-06-12: check/baseline/drift → `cli/check_command`, diff → `cli/diff_command`, preview → `cli/preview_commands`, discovery → `config::discover`)*
- [x] Push SF.8 to correct guard semantics. *(2026-06-11: a pair `#ifndef NAME` + `#define NAME` is required; a lone `#ifndef` (NDEBUG-tweak) no longer counts as a guard; the fixture `fail_ifndef_no_define/` + 3 unit tests)*

### P2 — sync the documents and the backlog

- [x] Close #045. *(closed, commit 87878fa)*
- [x] Update `AGENTS.md`. *(closed in #082 Slice 3: full rewrite)*
- [x] Normalize the duplication roadmap and remove the stale `#053` narrative. *(closed: #053 → `backlog/dropped/`, #085 completed, duplication = shipped advisory per #082 Slice 5b)*
- [x] Clean up `wip`/`new` where the links lead to deleted artifacts. *(closed in #082 Slice 4: 26 broken links fixed)*

### P3 — research vs product hygiene

- [x] Lock in a policy for `experiments/`. *(closed: `experiments/` in `.gitignore` as a local sandbox, outside git)*
- [x] Limit the use of experimental artifacts as a source of truth for shipped behavior. *(closed in #082: the current docs/AGENTS/roadmap don't reference `experiments/`)*

## Done

### Rebase of the inventory (2026-06-11)

The task was written 2026-06-02; the umbrella **#082** (2026-06-05, completed) closed
most of the items: `--config` → validate-only (the decision is fixed in #082),
diagnostics → `file:line`, the AGENTS.md rewrite, link hygiene, the duplication status,
plus #045 (commit 87878fa) and backlog passes (#053 → dropped/, #085). Of the 10 items,
the ones still live on 2026-06-11 were only #3 (diff thresholds), #4 (main.cpp), #5 (SF.8), #6 (discovery cwd).

### `check` vs `--diff` thresholds (item 3) — 2026-06-11

- `MetricThresholds::godHeaderFanIn` 30 → 50 (= the `config::Thresholds` default), an anchor comment in the header.
- `--diff` now discovers `.archcheck.yml` from the repo root and passes
  `thresholds.godHeaderFanIn` into `buildRegressionReport` (previously — a hidden literal 30);
  `ConfigError` in diff mode → exit 2 (the contract).
- Test-pin: `MetricThresholds{} == Config{}.thresholds` (git_diff_test.cpp).

### Config discovery from root (item 6) — 2026-06-11

- `discoverConfig(root)`: walkup from the analyzed root, not from the process CWD;
  `archcheck /path/to/project` picks up the project config from any launch location.
- `docs/config_format.md`: the discovery wording + the explicit precedence
  `--config` > discovered-near-root > embedded defaults.

### SF.8 guard semantics (item 5) — 2026-06-11

- A guard = a pair `#ifndef NAME` + a subsequent `#define NAME` of the same macro within a window
  of 60 non-empty lines (several pending `#ifndef`s are allowed, a `#define` of a different name doesn't close the guard).
- A lone `#ifndef` (the typical `#ifndef NDEBUG` case) no longer passes as a guard — the false negative is eliminated.
- `#pragma once`, BOM, ObjC, `.inc` — behavior unchanged; dogfood `src/ include/ tests/` — 0 violations.

Verification: build Debug, 465/465 tests, dogfood 0 violations, smoke `--diff` on
a real revspec, lizard/clang-format clean.

### Splitting `main.cpp` (item 4) — 2026-06-12

A behavior-preserving refactor in four slices (each ≤2 new files,
build+tests green after each):

- **c1** — `config::discover(root)` moved into the config subsystem
  (`config_loader.{h,cpp}`): a shared discovery policy for check and diff,
  +2 unit tests (walkup-found config / embedded defaults).
- **c2** — `src/cli/check_command.{h,cpp}`: `OutputFormat`/`BaselineMode`/`BaselineOpts`,
  baseline save/load/filter, drift-gate policy, `runCheck`, `runSaveGraphBaseline`.
- **c3** — `src/cli/preview_commands.{h,cpp}`: `runScan`/`runGraph`/`runDuplication`/`runHistory`.
- **c4** — `src/cli/diff_command.{h,cpp}`: `DiffMode`, `runDiff` + advisories
  (SATD, test co-evolution, local complexity drift).

`main.cpp`: 786 → 275 lines — only help/version, argv parsing (`dispatch_*`),
`main()`. The CLI units are internal headers `src/cli/*.h` (not in `include/archcheck/` —
not a public API), the executable got `target_include_directories(PRIVATE src/)`.
The moved functions were renamed to `lowerCamelCase` per code_style (`run_check` →
`cli::runCheck`); the bodies weren't changed. Known legacy: the lizard warning on
`applyBaselineAndReport` (CCN 12, 38 lines) existed in main.cpp too — not new debt.

Verification: build Debug, 467/467 tests, dogfood `src/ include/ tests/` 0 violations
(the new headers pass SF.7/8/9), smoke of all CLI modes (check/json/config/
baseline save+load/graph-baseline/drift/scan/graph/diff), clang-format 18.1.3 clean.

## Acceptance criteria

- [x] The CLI user contract no longer promises behavior that isn't there. *(#082)*
- [x] `check` and `--diff` use consistent thresholds/policy. *(2026-06-11)*
- [x] For config discovery there's no dependency on a random `cwd`. *(2026-06-11)*
- [x] `docs/MVP.md`, `docs/ROADMAP.md`, `docs/architecture-spec.md`, `AGENTS.md` don't contradict the shipped v0.1/v0.2 scope. *(#082 + #045)*
- [x] No tasks remain in `backlog/wip/` whose current state contradicts the architectural decisions or references deleted trees. *(#082 Slice 4 + backlog passes, #053 → dropped/)*
- [x] `src/main.cpp` stops being the place where all the application policy is concentrated. *(2026-06-12: thin dispatch, 275 lines)*

## Key decisions

| Decision | Reason |
|----------|--------|
| Make one umbrella task, not a scatter of micro-tickets | the tech debt here is systemic and causally connected; on its own it loses context |
| Don't duplicate #082's work — rebase the inventory before starting | #082 (XL umbrella, 2026-06-05) closed items 1,2,7,8,9,10; only items 3,4,5,6 remain here |
| The diff-threshold default aligned to 50 (= check), not the other way | 50 is the documented authoritative Lakos.GodHeader threshold (help, config_format.md); 30 was a hidden literal in the diff layer |
| Diff discovers the config from the repo root (the worktree version of `.archcheck.yml`) | the worktree state is the project's current policy; the config from the baseline ref isn't readable |
| SF.8: a pair ifndef+define, without supporting `#if !defined(...)` | covers the declared guard semantics without scope creep; the `!defined` form wasn't accepted before either |
| Alignment first, then new rules | while the basic contracts are untrustworthy, every new feature raises the support cost |
| Don't mix docs cleanup with product cleanup | some items require a behavior decision, not text rewriting |
| Treat the duplication stale-state as tech debt, not "just research" | the backlog/current docs already consume this narrative as if it were product-relevant |

## Changed files

| File | Change |
|------|--------|
| `backlog/wip/073_maj_tech_debt_alignment_cleanup.md` | the umbrella task; 2026-06-11 rebase against #082 + progress log |
| `include/archcheck/diff/regression_report.h` | default `godHeaderFanIn` 30 → 50 + an anchor comment to config.h |
| `src/main.cpp` | `discoverConfig(root)`; diff passes the thresholds from the config, ConfigError → exit 2 |
| `tests/integration/diff/git_diff_test.cpp` | a test-pin on the equality of check/diff defaults |
| `docs/config_format.md` | discovery from the analyzed root + an explicit precedence |
| `src/rules/sf8_include_guard.cpp` | a guard = a pair `#ifndef`+`#define` of one macro (`directiveArg`) |
| `tests/unit/rules/sf8_include_guard_test.cpp` | 3 tests: lone ifndef, a foreign define, a comment between the pair |
| `fixtures/sf8_include_guard/fail_ifndef_no_define/ndebug_tweak.h` | a false-guard fixture (`#ifndef NDEBUG`) |
| `include/archcheck/config/config_loader.h`, `src/config/config_loader.cpp` | c1: `config::discover(root)` — shared discovery policy |
| `tests/unit/config/test_loader.cpp` | c1: 2 tests for `discover` |
| `src/cli/check_command.{h,cpp}` | c2: check + baseline/drift/report policy |
| `src/cli/preview_commands.{h,cpp}` | c3: scan/graph/duplication/history |
| `src/cli/diff_command.{h,cpp}` | c4: diff mode + advisories |
| `src/main.cpp` | 786 → 275 lines: help/version + thin argv-dispatch |
| `src/CMakeLists.txt` | cli/*.cpp into the executable, PRIVATE include on src/ |

## How it works

The umbrella was closed in three layers. (1) Contract fixes: the `MetricThresholds`
default is aligned to 50 and pinned by a test to `config::Thresholds`, `--diff` takes the threshold from
the discovered config; discovery (`config::discover(root)`) goes walkup from the
analyzed root, not the process CWD. (2) SF.8: a guard is counted only on
a pair `#ifndef NAME` + `#define NAME` (the helper `closesGuardPair`, a window of 60 non-empty
lines) — a lone `#ifndef` no longer passes. (3) Application layer: `main.cpp` —
thin dispatch (help/version + argv parsing), all command policy is in the internal
units `src/cli/` (`check_command`, `diff_command`, `preview_commands`).

## Result

**Status:** completed
**Completed:** 2026-06-12

Of the 10 inventory items, 6 were closed by the umbrella #082 (+#045, backlog passes) —
fixed by the rebase; the remaining 4 (diff thresholds, discovery root, SF.8,
main.cpp) are closed here. All acceptance criteria are met. Verification: build
Debug, 467/467 tests, dogfood `src/ include/ tests/` 0 violations, smoke of all
CLI modes, clang-format 18.1.3 / cppcheck / lizard clean, coverage
92.8/96.3/59.8 PASS. Known legacy: the lizard warning `applyBaselineAndReport`
(CCN 12) — moved out of main.cpp as is, not new debt.
