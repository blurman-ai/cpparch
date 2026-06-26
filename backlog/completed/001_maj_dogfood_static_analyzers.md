# [BUILD] Dogfood: cppcheck + clang-tidy + lizard in CI

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** BUILD
**Priority:** major
**Complexity:** S (a day)
**Blocks:** —
**Blocked by:** ~~#002 (github_actions_ci)~~ — unblocked
**Related:** #005 (sarif_reporter_spec)

## Goal

Don't let archcheck's code rot — run three cheap external tools on top of clang-tidy in every PR: **cppcheck**, **lizard**, and a second clang-tidy pass with an extended set. A full report from archcheck on itself is a separate task, once rules exist.

## Context

From research 2026-05-26: cppcheck catches what clang-tidy misses (UB, leaks via a different analyzer), lizard — complexity thresholds (CCN ≤ 15, function ≤ 30 lines — matches `docs/code_quality.md`). Both are free, fast, low noise. scan-build and `-fanalyzer` — later, in nightly.

## Execution plan

- [x] Separate `static-analysis` job in `ci.yml`, parallel to the build (one runner, ubuntu-24.04)
- [x] **cppcheck:** `cppcheck --enable=warning,performance,portability --inline-suppr --error-exitcode=1 src/ include/` — fails on findings. Locally on our code: 0 findings.
- [x] **lizard:** `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` + grep for `warning:` / `!!!` — fails on threshold exceedance (lizard itself always exits 0). Locally on our code: 0 findings.
- [x] **clang-tidy second pass** with `.clang-tidy-strict` (bugprone, performance, modernize, cppcoreguidelines, readability, cert, misc, hicpp, llvm). Runs with `continue-on-error: true` — **warning-only**, accumulates a baseline, does not fail the build.
- [x] Save reports (`lizard-report.txt`, `clang-tidy-strict-report.txt`) as artifacts for 14 days
- [x] Do **not** enable IWYU — we are in that niche ourselves
- [x] Do **not** enable flawfinder — outdated, almost everything is off-target for us

## Done

- **2026-05-26** — Set up `.clang-tidy-strict` (cert-*, hicpp-*, misc-*, llvm-* on top of the regular set). Noisy checks (`llvm-header-guard`, `llvm-include-order`, `hicpp-signed-bitwise`, `cert-err58-cpp`, `cppcoreguidelines-pro-bounds-*`) are explicitly disabled.
- **2026-05-26** — Added the `static-analysis` job to `.github/workflows/ci.yml` (parallel to `build`, ubuntu-24.04):
  - apt install: `cppcheck`, `clang-tidy-18`, `g++-13`, `ninja-build`, `cmake`; pip install: `lizard`.
  - Cache `build/debug/_deps` under a separate key `deps-Linux-static-...` (isolated from the build cache, otherwise a race condition on parallel write).
  - `cmake configure` to generate `compile_commands.json` (needed by clang-tidy).
  - **cppcheck**: gate (`--error-exitcode=1`).
  - **lizard**: gate with a grep check for `warning:`/`!!!` (lizard itself always exits 0).
  - **clang-tidy strict**: `continue-on-error: true`, prints the report into the log in a `clang-tidy strict report` group, accumulates a baseline.
  - Upload `lizard-report.txt` + `clang-tidy-strict-report.txt` as an artifact for 14 days.
- **2026-05-26** — **Local verification**: cppcheck + lizard run on `src/`, `include/`, `tests/` — both green (exit 0, no findings).
  - clang-tidy not checked locally — the machine has clang-tidy 11, which does not support `--config-file=` (that's from 12+). CI uses clang-tidy-18.

## In progress
- (empty)

## Next steps

1. cppcheck job — the cheapest, start with it
2. lizard job
3. clang-tidy strict as warning-only

## Key decisions

| Decision | Reason |
|---------|---------|
| cppcheck + lizard as a gate, clang-tidy strict as warning | Minimum noise at the start, otherwise we'll drop it |
| scan-build/-fanalyzer — later | More expensive in time, in nightly once CI stabilizes |

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| `.clang-tidy-strict` | new — bugprone + performance + modernize + cppcoreguidelines + readability + cert + misc + hicpp + llvm | `1f06811` |
| `.github/workflows/ci.yml` | new `static-analysis` job parallel to `build` | `1f06811` |
| `.github/workflows/ci.yml` | bump actions/upload-artifact@v4 → v7 (Node 24) | `0a2a619` |

## How it works

The CI workflow now has two independent jobs running in parallel:

1. **`build`** (gcc-13 / clang-18 matrix) — configuration, compilation, ctest, binary smoke. Green = code built and tests passed.
2. **`static-analysis`** (single runner) — three tools on top of the same `compile_commands.json`:
   - **cppcheck** — `--enable=warning,performance,portability`, `--inline-suppr`, `--error-exitcode=1`. Fails on any finding in these categories.
   - **lizard** — thresholds from `docs/code_quality.md`: `CCN ≤ 15`, function ≤ 30 lines, ≤ 5 arguments. Lizard itself always exits 0; the step greps the report for `warning:` / `!!!` and explicitly fails if found.
   - **clang-tidy strict** — with `.clang-tidy-strict` (extended check set), `continue-on-error: true`. Does not fail the build, prints the report into the log in a `::group::`, accumulates a baseline for later tightening. Output also goes to an artifact.

**Cache.** The static-analysis job shares cmake configure with build (needed for compile_commands.json), but caches FetchContent in a separate scope `deps-Linux-static-...` to avoid a write-race with the build job, which may concurrently pull ryml/Catch2.

**Artifacts.** `static-analysis-reports` (lizard + clang-tidy-strict reports) are kept for 14 days. Available via the link to the right of the run on the Actions page.

**What is NOT included:**
- IWYU — we are in that niche ourselves.
- flawfinder — outdated, near-zero signal for our patterns.
- scan-build / `-fanalyzer` — too slow for per-PR, candidates for a nightly workflow when we get to it.

## What controls it

| Parameter | Where it's changed |
|---|---|
| lizard thresholds | `--CCN`, `--length`, `--arguments` in the workflow step. Matches `docs/code_quality.md` — change in sync. |
| cppcheck categories | `--enable=...` in the workflow step. To add style: `warning,performance,portability,style`. |
| clang-tidy strict checks | `.clang-tidy-strict` — `Checks:` key. |
| Noisy checks in strict | List `-check-name` in `.clang-tidy-strict`. As things stabilize — move them into the regular `.clang-tidy` as a hard error. |
| Report retention | `retention-days: 14` in the upload-artifact step. |

## Related to

- **#002 (github_actions_ci)** — static analysis lives in the same workflow `.github/workflows/ci.yml`, as a parallel job. When changing the main build job — check that the static-analysis cache doesn't conflict.
- **#004 (project_skeleton)** — static analysis depends on `.clang-tidy` (regular, in build) and `.clang-tidy-strict` (new, in static-analysis). Both are configured for the layout from #004.
- **Future task `nightly_deeper_analysis`** — for `scan-build`, `-fanalyzer`, ASan/UBSan on full-scale projects. Not a blocker, set it up when pain from false negatives in the current set appears.
- **`docs/code_quality.md`** — lizard thresholds come straight from this document. If we change thresholds in one place — sync the other immediately.

## Diagnostics

```bash
# Local reproducibility (on the developer's machine):
cppcheck --enable=warning,performance,portability --inline-suppr \
  --error-exitcode=1 --suppress=missingIncludeSystem --quiet \
  -I include src/ include/

lizard --CCN 15 --length 30 --arguments 5 --warnings_only \
  src/ include/ tests/

# clang-tidy strict requires CMake-configure (for compile_commands.json)
# and clang-tidy >= 12 (--config-file= support):
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
clang-tidy --config-file=.clang-tidy-strict -p build/debug \
  $(find src/ -name '*.cpp')

# CI artifact:
# https://github.com/blurman-ai/cpparch/actions  ->  select a run  ->
# on the right "Artifacts" -> static-analysis-reports
```

If cppcheck/lizard fails locally but is green in CI — usually it's a difference in tool versions (apt cppcheck on Astra 1.7 — 1.86, on ubuntu-24.04 — 2.13+; lizard via pip — the same everywhere).
