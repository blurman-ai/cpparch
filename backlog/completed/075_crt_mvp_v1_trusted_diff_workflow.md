# [CLI][DIFF][REPORT] MVP v1: bring the trusted PR/diff workflow to product-grade

**Creation date:** 2026-06-02
**Start date:** 2026-06-12
**Status:** wip
**Module:** CLI / DIFF / REPORT / DOCS
**Priority:** critical
**Difficulty:** M
**Blocks:** MVP v1 release readiness
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #023 (diff_skip_when_no_cpp_changes), #024 (in_memory_fs_for_diff), #025 (pr_comment_integration), #028 (rules_engine_mvp), #030 (baseline_file_flag), #073 (tech_debt_alignment_cleanup), #045 (docs_sync_roadmap_mvp_spec)

## Goal

Assemble the already-existing pieces of `--diff` / baseline / report / CI into **one
canonical user workflow** that can be trusted as the first
product wedge of `archcheck`.

The task is not about new graph metrics, but about **productization**:

- one clear run scenario in CI;
- one clear report;
- one clear exit-code behavior;
- an explicit boundary between advisory and gated mode.

## Context

The foundation already ships:

- `#018` delivered `--diff`;
- `#023` and `#024` made the diff path fast;
- `#025` showed the path into PR/CI;
- `#030` delivered baseline;
- the rules/report core already lives in the product.

But the remainder is still spread across several tasks and files. Right now there is
a technical implementation of diff analysis, but no single clear product story at the level of:

> "Drop one command into CI and learn whether this PR introduced a new bad dependency."

Until that exists, the MVP looks like a set of features rather than an assembled product path.

## What should result

For the user there should exist **one canonical workflow**:

- ran diff against base;
- saw which new dependencies/cycles appeared;
- understood whether it is advisory or a gated regression;
- got stable text/json output;
- could copy a ready CI example without reading half-implemented surfaces.

## What to do

### 1. Lock down the canonical diff surface

- [x] Pick and document one primary form of the PR/diff analysis run:
      `archcheck --diff origin/main..HEAD .` (README Quick start, --help, example yml).
- [x] Explicitly describe how this form relates to the baseline-flow and the local run
      (README: `--drift-baseline` = an alternative with a pinned snapshot file).
- [x] Remove from docs/help the ambiguity between "several similar modes" and
      "one primary product path".

### 2. Lock down the report's product contract

- [x] Define the mandatory composition of the diff report for MVP v1:
      added edges / new cycles / SCC growth / brief summary (+ gate verdict).
- [x] Make `text` and `json` stable and deterministic
      (`--diff --format=json`, schema version 1, new `diff_json_report`).
- [x] Remove from default output everything that cannot yet be trusted as a trusted gate:
      all non-gating sections marked `(advisory)`, gating ones — `(gating)`.

### 3. Explicitly separate advisory and gate

- [x] Lock down the advisory-first default.
- [x] Gated in MVP v1 (user decision 2026-06-12): new/grown cycles
      (≈SF.9) + new god-headers (≈Lakos.GodHeader). Added edges, cross-area,
      chain/NCCD growth, SATD/test-coevol/LCX — advisory. Strict mode — YAGNI.
- [x] Tied to exit codes without breaking the contract: `RegressionReport::gates()`,
      exit 1 only on gated; the text report ends with `gate: ok|fail`.

### 4. Build E2E coverage of the workflow

- [x] Product-level scenarios: clean diff, new edge (advisory, exit 0),
      new cycle (gate, exit 1), docs-only fast-path; the same for json.
- [x] The final user output of the real binary is checked
      (`tests/integration/diff/diff_workflow_e2e_test.cpp`, ARCHCHECK_BINARY_PATH).

### 5. Provide minimal CI onboarding

- [x] One copy-paste example: `.github/workflows/example_archcheck_pr.yml`
      (exit codes and advisory semantics updated), linked from README.
- [x] Strict gate mode — decision: don't do it (YAGNI), one contract.
- [x] README / MVP.md / CHANGELOG / --help describe one and the same path.

## Acceptance criterion

- [x] `archcheck` has one canonical PR/diff workflow, described identically in code/docs.
- [x] Default behaviour is advisory-first, without hidden gating surprises.
- [x] `text/json` output is stable and verified by e2e fixtures.
- [x] There is a copy-paste CI example without reliance on research/docs archaeology.
- [x] The user's answer to the question "what does this PR do to the dependency graph?" is obtained with one command.

## Log

- 2026-06-12: surface audit. Main gap: `hasRegression()` gated ANY added
  edge / NCCD growth → every live PR red (contradicting advisory-first). Also:
  no JSON for `--diff`, e2e checked the DTO, README presented `--drift-baseline` and
  `--diff` as parallel modes. Along the way fixed the README: the Default
  thresholds table was glued inside the bash code block of Quick start.
- 2026-06-12: implementation (all in one pass, gates green):
  - `gates()` in `RegressionReport` (cycles + god-headers); exit 1 only on gates();
    text-report sections marked `(advisory)`/`(gating)`; verdict `gate: ok|fail`.
  - `--diff --format=json`: new `diff/diff_json_report.{h,cpp}` (schema v1:
    refs / gate / gating{} / advisory{} with flattened SATD+coevol+LCX violations);
    fast-path also emits valid JSON. `consumeDiffFlags` in main.cpp understands
    `--diff-mode=` and `--format=` in any order.
  - diff_command refactor: collect/print separated (DiffAdvisories),
    `loadDiffThresholds`/`emitJsonDiff` extracted (lizard limits).
  - E2E: `tests/support/git_test_repo.h` (shared header; migration of two old
    TempDir copies — in #108) + `diff_workflow_e2e_test.cpp` — 6 cases via
    the real binary (`$<TARGET_FILE:archcheck>`), text+json, exit codes.
  - Docs: README (canonical PR block, code-block fix, link to example yml),
    MVP.md §4, CHANGELOG (Added: JSON; Changed: advisory-first), example yml,
    --help.
  - Gates: 1688 assertions / 491 cases green; dogfood src/include/tests = 0;
    clang-format 18.1.3 clean; lizard — only pre-existing debt (#108).
  - NB: the build was done in `build/dev075` (a separate build dir) — `build/debug`
    is occupied by the background run #109; before closing #109, rebuilding `build/debug`
    is safe (the LCX script parses only the `local complexity drift
    (advisory):` section, the format hasn't changed, the exit code isn't used).

- 2026-06-12 (closing): the coverage gate first failed (85.5% < 90%) — the e2e for the first
  time forced the coverage run to execute the CLI binary, and the entire previously-unmeasured
  CLI layer entered the base with zeros. By the user's decision a CLI smoke e2e was added
  (`tests/integration/cli/cli_smoke_e2e_test.cpp`, 10 cases: help/version/check/
  json/baselines/previews/dispatch errors) → 91.4% lines / 96.5% functions / 57.6%
  branches, PASS. Along the way: cppcheck 1.86 didn't parse the init-statement in `else if`
  (`local_complexity_drift.cpp`, code from #109) — rewritten without the init-statement
  (GCC8-COMPAT marker); the binary-launch helper was extracted into
  `tests/support/run_archcheck.h` (dedup between two e2e files).

## How it works

`--diff a..b` builds the include graphs of both sides (in-memory via git blobs or
disk worktree), compares them in `RegressionReport`, and prints a text or
JSON report. The gate is `RegressionReport::gates()`: exit 1 only on new/grown
cycles or new god-headers (what in check mode would be an SF.9 /
Lakos.GodHeader violation); all other deltas and scan signals (SATD, test co-evolution,
LCX) are printed as advisory and don't affect the exit code. The JSON contract (schema v1,
`diff_json_report.cpp`) separates the `gating` block from `advisory` and closes with the
field `gate: ok|fail`. E2E tests run the real binary (`$<TARGET_FILE>`)
on synthesized git repos and check the final stdout + exit code.

## Key decisions

- **Gated = cycles + god-headers, without a strict flag** (user, 2026-06-12):
  mirrors `--drift-baseline` (DRIFT.1/2), rule-equivalent classes; added edges —
  the normal life of a PR, gating them = red on every `#include` (the anti-pattern
  "5000 violations"). Strict mode — YAGNI until a request from a real user.
- **`hasRegression()` kept** (= "there is something to show"), `gates()` added
  alongside — 15 existing tests didn't require rethinking the semantics.
- **JSON for diff — a separate module**, not an extension of `report::writeJsonReport`:
  check and diff have different documents (a violations list vs a gating/advisory structure).
- **E2E via the real binary**, not via linking the cli into tests: it checks
  precisely the product path (dispatch, flags, exit codes), and along the way coverage saw
  the CLI layer.

## Changed files

- `include/archcheck/diff/regression_report.h`, `src/diff/regression_report.cpp` —
  `gates()`, `(advisory)`/`(gating)` markers, verdict `gate: ok|fail`.
- `include/archcheck/diff/diff_json_report.h`, `src/diff/diff_json_report.cpp` —
  JSON document schema v1 (new).
- `src/cli/diff_command.{h,cpp}` — exit by `gates()`, collect/print separation,
  `OutputFormat` parameter, JSON fast-path.
- `src/main.cpp` — `consumeDiffFlags` (`--diff-mode=` + `--format=`), help.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt` — registration; e2e get
  `ARCHCHECK_BINARY_PATH`.
- `tests/unit/diff/regression_report_test.cpp` — gates() asserts, new markers.
- `tests/support/git_test_repo.h`, `tests/support/run_archcheck.h` — shared
  e2e scaffolding (new).
- `tests/integration/diff/diff_workflow_e2e_test.cpp`,
  `tests/integration/cli/cli_smoke_e2e_test.cpp` — product E2E (new).
- `README.md`, `docs/MVP.md`, `CHANGELOG.md`,
  `.github/workflows/example_archcheck_pr.yml` — one canonical workflow.

## Outcome

**Status:** completed
**Completion date:** 2026-06-12

## Not to do in this task

- don't drag duplication into the release gate;
- don't extend the product toward AI-attribution;
- don't do runtime config policy (`layers` / `forbidden` / `independence`) — that's already `v0.2`;
- don't mix with semantic backend / libclang expansion.

## Next steps

1. First close the `#073` P0 slice on contracts.
2. After that, assemble and lock down the canonical diff contract.
3. Only then pick up richer metrics and optional integrations.
