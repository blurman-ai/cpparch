# [RULES][CONFIG] Audit of hardcoded strings and constants

**Creation date:** 2026-05-28
**Start date:** 2026-05-30
**Completion date:** 2026-06-11
**Status:** completed
**Module:** RULES, CONFIG
**Priority:** major
**Complexity:** M-L (refactor of defaults into Config + walkup + merge; threshold override via YAML = format extension phase 2)
**Blocks:** —
**Blocked by:** —
**Related:** docs/config_format.md (§Discovery & default resolution — model locked), completed/051_maj_config_loader_v1.md (phase-1 loader), completed/v1_maj_config_format_minimal_contract.md

## Goal

Inventory all hardcoded strings and constants in the code (file patterns, threshold values, violation texts), classify each one, and move the "overridable" ones into `Config` or into constants with explicit intent.

## Context

Under the "zero-config" banner, three kinds of contextual strings leak into the code:

1. **File patterns** — extensions `.h`, `.hpp`, `.inc`, paths to system headers — may not match the user's actual project.
2. **Threshold values** — `chain_length=10`, `god_header_fan_in=50` — baked into the rule implementations (`kDefaultThreshold` in each rule's `.h`), but they should be overridable via `.archcheck.yml`. (NB: `brace_depth=1` from the original wording is SF.7 parsing mechanics, not a setting; we don't extract it.)
3. **Violation texts** — the violation message contains specific context ("using namespace in a header file") — this is fine, but the format must be consistent.

"Zero-config" means "good defaults", not "immutable magic numbers". A user with a non-standard project should not have to patch the sources.

### Target architecture: Embedded Defaults + File Override

The pattern used by clang-tidy, rustfmt, clippy — exactly what's needed for a single static binary:

```
Hardcoded Config defaults  →  merge  ←  .archcheck.yml (if present)
                                ↓
                          Final Config  →  rules
```

1. All defaults live in named fields of the `Config` struct — a single source of truth
2. On startup we search for `.archcheck.yml` up the tree from CWD
3. The found file is merged **on top of** the defaults (the user writes only what they change)
4. `Config` is passed into each rule through the `IRule` interface

The problem isn't that the defaults are baked into the binary — that's how it should be. The problem is that they are **scattered** across rule implementations instead of a single `Config` struct.

## Execution plan

- [ ] Grep `src/` and `include/`: collect all string literals and numeric constants related to rules
- [ ] Build an inventory table: string / file:line / category
- [ ] Categories:
  - `FIXED` — a constant by the standard (e.g., `#pragma once` — that's literally C++ syntax)
  - `DEFAULT` — a correct default, scattered across the code; needs to be moved into the `Config` struct
  - `INLINE_OK` — a violation message, embedded fine, but the format needs to be consistent
- [ ] Create / extend the `Config` struct: one field per `DEFAULT`, with a default value
- [ ] Make sure `IRule::check()` receives `Config` (or a subset) — does not read global constants
- [ ] Implement search and merge of `.archcheck.yml` on top of defaults
- [ ] Document all defaults in `README.md` in the "Default thresholds" section and in `archcheck --help`

## Done

- **Inventory of hardcoded constants** (2026-05-30):

  | What | Value | Where now | Category |
  |-----|----------|-----------|-----------|
  | include chain length | `10` | `include/archcheck/rules/lakos_chain_length.h:11` (`kDefaultThreshold`) | DEFAULT |
  | god-header fan-in | `50` | `include/archcheck/rules/lakos_god_headers.h:14` (`kDefaultThreshold`) | DEFAULT |
  | project extensions | `.c .cc .cpp …` | `src/scan/project_files.cpp:16` (`kProjectExtensions`) | DEFAULT |
  | header extensions | `.h .hpp …` | `src/scan/project_files.cpp:19` (`kHeaderExtensions`) | DEFAULT |
  | `#pragma once` / guard syntax | — | `src/rules/sf8_include_guard.cpp` | FIXED |
  | `.inc` = single-include | — | `src/rules/sf8_include_guard.cpp:52` | FIXED |
  | SF.7 brace-depth | `1` | `src/rules/sf7_using_namespace.cpp` | FIXED (parsing) |
  | violation texts | — | all rules | INLINE_OK |

- **Design locked** in `docs/config_format.md` §"Discovery & default resolution" and in project memory (`project_config_discovery_defaults`): embedded-defaults + file-override; walkup from CWD to FS-root (first file wins); `--config` disables walkup; merge on top of defaults.
- **Step 1 (2026-05-30):** `Config::thresholds` (`struct Thresholds { chainLength=10; godHeaderFanIn=50; }`) introduced in `include/archcheck/config/config.h` as the single source of truth.
- **Step 2 (2026-05-30):** `Config` plumbed through the pipeline. `makeDefaultRuleSet(const Config&)` passes `config.thresholds` into the `LakosGodHeaders`/`LakosChainLength` constructors; `run_check` takes `Config`; `dispatch_config` no longer throws away the loaded config (`(void)load` → passed into `run_check`). 248/248 tests green. Rules' `kDefaultThreshold` kept as the constructor default (it's pinned by tests + direct construction) — the literals `10/50` are now duplicated in two places, dedup is a minor follow-up.
- **Step 3 (2026-05-30):** the loader parses the optional `thresholds:` block (`chain_length`, `god_header_fan_in`), validates positive ints, merges on top of defaults; an unknown/non-positive key → exit 2. Fixtures (`pass/thresholds`, `fail_unknown_threshold_key`, `fail_threshold_not_positive`) + 4 tests. `docs/config_format.md` updated (`thresholds` section, removed from "not in phase 1"). **End-to-end override works:** `chain_length: 2` on a deep chain → violations, default → clean. 252/252 tests, coverage 95.0% PASS.
- **Step 4 (2026-05-30):** `config::findConfig(start)` — walkup from CWD to FS-root, the first `.archcheck.yml` wins. `run_check` takes `optional<Config>`: set explicitly (`--config`) → used as-is (walkup skipped); not set → discovery + load (config error → exit 2). Discovery extracted into `discoverConfig()`. Fixtures `discovery/.archcheck.yml` + `discovery/nested/deep/.gitkeep`, 2 tests (found/nullopt). **Zero-config override works:** an `.archcheck.yml` in a parent is picked up from a subdirectory WITHOUT `--config`. 257/257 tests, coverage 95.0% PASS.

## In progress

- (empty)

## Completed (2026-06-11)

### Increment A — documenting the defaults ✅

- README.md: added a "Default thresholds" subsection with a 4-row table (values copied from docs/config_format.md §"Where defaults live")
- src/main.cpp: help text extended with a line about the defaults chain_length=10, god_header_fan_in=50, override via thresholds: in .archcheck.yml
- DoD A:
  - The values in README match docs/config_format.md byte for byte ✅
  - `archcheck --help | grep -c "chain_length"` = 1 ✅
  - Build (Debug) successful ✅
  - All tests: 411/411 ✅

### Increment B — dedup of the literals 10/50 ✅

- include/archcheck/rules/lakos_chain_length.h: added `#include "archcheck/config/config.h"`, constructor default replaced with `config::Thresholds{}.chainLength`, `kDefaultThreshold` removed
- include/archcheck/rules/lakos_god_headers.h: added `#include "archcheck/config/config.h"`, constructor default replaced with `config::Thresholds{}.godHeaderFanIn`, `kDefaultThreshold` removed
- tests/unit/rules/lakos_chain_length_test.cpp: added include config.h, CHECK(LakosChainLength::kDefaultThreshold == 10) replaced with CHECK(archcheck::config::Thresholds{}.chainLength == 10)
- tests/unit/rules/lakos_god_headers_test.cpp: added include config.h, CHECK(LakosGodHeaders::kDefaultThreshold == 50) replaced with CHECK(archcheck::config::Thresholds{}.godHeaderFanIn == 50)
- DoD B:
  - grep -rn "kDefaultThreshold" = 0 lines ✅
  - All tests: 411/411 ✅
  - Lizard: 0 new warnings ✅
  - Dogfood: 0 violations on src/, include/, tests/ ✅
  - Increment A: 12 insertions (+) in README.md and src/main.cpp ✅ ≤50
  - Increment B: 11 insertions (+), 8 deletions (-) in rule headers and tests ✅ ≤50

## Next steps

> ⚠️ **Scope:** threshold override via YAML = the `thresholds:` block, and that is **v1 phase 2** per `docs/config_format.md`. The phase-1 loader (#051) knows only `version`/`modules`/`rules`. This task adds `thresholds:` (additive/MINOR, `version: 1` preserved) + refactor of defaults + walkup.

1. ✅ Introduce fields in the `Config` struct for the DEFAULT constants (`thresholds`) with in-code defaults.
2. ✅ Pass `Config` into the rules (`makeDefaultRuleSet(const Config&)`); plumbing through `run_check`; `dispatch_config` no longer throws away the config.
3. ✅ Parse the `thresholds:` block in the loader + merge on top of defaults; `docs/config_format.md` updated; fixtures + tests; end-to-end override works.
4. ✅ Walkup search for `.archcheck.yml` from CWD to FS-root (`findConfig`); `--config` disables it; zero-config override works.
5. ⚠️ **Narrowed by decision 2026-06-11**: we do NOT do YAML override of extensions. Rationale: (a) the constant dedup is already done — `kProjectExtensions`/`kHeaderExtensions` live in one place `include/archcheck/scan/file_classification.h` (the "Task #041" comment is there too); (b) extension override is absent from ALL phases of `docs/config_format.md` §"What is not in v1 phase 1" — adding a new top-level key without a user request violates YAGNI. The remainder of step 5 = documentation of defaults only (increment A below).
6. Dedup of the literals `10/50` (increment B below).

## Plan for Haiku (2026-06-11) — two increments, close the task entirely

Before starting you MUST read in full: this task, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2, [docs/code_quality.md](../../docs/code_quality.md).

### Increment A — documentation of defaults (README + `--help`)

Facts (verified 2026-06-11): the `--config` example is at `README.md:62-63`; the help text is in `src/main.cpp` (~lines 50-70, the usage-printing function); the canonical default values — the table in `docs/config_format.md` §"Where defaults live" (chain length 10, god-header fan-in 50, project/header extensions).

1. In `README.md` next to the `--config` example (after line ~63) add a "Default thresholds" subsection — a 4-row table, **copy the values from `docs/config_format.md`**, not from memory. Mention: override via the `thresholds:` block in `.archcheck.yml`.
2. In the help text of `src/main.cpp` add 1–2 lines: the defaults `chain_length=10`, `god_header_fan_in=50`, override via `thresholds:` in `.archcheck.yml`.

DoD of increment A:
- the values in README match `docs/config_format.md` byte for byte (10, 50, the extension lists);
- `~/projects/cpparch/build/debug/src/archcheck --help | grep -c "chain_length"` ≥ 1;
- build + all tests green.

### Increment B — dedup of the literals 10/50 (former step 6)

Facts (verified 2026-06-11): `kDefaultThreshold = 10` in `include/archcheck/rules/lakos_chain_length.h:11`, `= 50` in `include/archcheck/rules/lakos_god_headers.h:14`; the same numbers — in `config::Thresholds` (`include/archcheck/config/config.h`). Tests pin the constant: `tests/unit/rules/lakos_chain_length_test.cpp:45`, `tests/unit/rules/lakos_god_headers_test.cpp:42`.

1. In both rule headers add `#include "archcheck/config/config.h"`; replace the constructor default with `config::Thresholds{}.chainLength` (resp. `.godHeaderFanIn`); remove `kDefaultThreshold`. The dependency direction rules→config is correct (rules already receive `Config` via `makeDefaultRuleSet(const Config&)`).
2. **Permitted edit of test expectations** (and only it): `CHECK(LakosChainLength::kDefaultThreshold == 10)` → `CHECK(archcheck::config::Thresholds{}.chainLength == 10)`; likewise for 50. The numbers 10/50 do NOT change — only the source changes. Do NOT touch any other expectations.

DoD of increment B:
- `grep -rn "kDefaultThreshold" include/ src/ tests/` → 0 lines;
- all tests green; lizard 0 warnings; dogfood 0 violations;
- each increment fits within ≤50 lines of diff.

After A+B the task is closed entirely (`/fix-issue`).

### Do not do

- Do NOT introduce a YAML key for extensions (decision above).
- Do NOT touch `file_classification.h`, the loader, `discoverFiles`.
- Do NOT commit without an explicit command.

### Escalation (when to stop and hand off to a higher model)

Stop, write here "Blocked: <what/why/what you tried>" and report if: including config.h into a rule header causes a circular dependency or breaks the build after 2 attempts; any test fails other than the two explicitly permitted for editing; you need a file outside those listed. Next — Sonnet, then Opus.

## Key decisions

| Decision | Reason |
|---------|---------|
| Defaults stay in the binary, require no file | Zero-config: works on the first run without `.archcheck.yml` |
| All `DEFAULT`s in a single `Config` struct | Single source of truth; clang-tidy/rustfmt/clippy use the same scheme |
| Merge `.archcheck.yml` on top of defaults | The user writes only the differences, the file is minimal |
| Config search up the tree (walkup) | Standard for CLI tools; convenient from project subdirectories |
| Violation texts stay inline | They are part of a rule's semantics, not a setting |

## Changed files

| File | Change |
|------|-----------|
| ... | ... |
