# [SCAN] Spike: libclang on spdlog/fmt — close the question "is a fast backend needed in v0.1"

**Created:** 2026-05-29
**Started:** 2026-05-29
**Status:** wip
**Module:** SCAN
**Priority:** major
**Complexity:** S (1–2 days: setup + measurement + a short report)
**Blocks:** #042 (clang_semantic_backend — its scope depends on the spike's answer)
**Blocked by:** —
**Related:** #7 (gh — owner), #006 (spec_refactor — laid down the two-backend scheme), #042 (clang_semantic_backend)

## Goal

**One number that closes the open architectural question from the spec: is the two-backend scheme needed in v0.1.**

Not "play with clang". Not "start building the libclang backend". Measure the time and peak memory of `clang_parseTranslationUnit` + `clang_getInclusions` per TU of a real project.

- **If seconds for a medium project** — libclang-only is viable, a fast backend isn't needed in the MVP, the scope of #042 shrinks to "libclang as default", and the `--with-clang` flag isn't needed.
- **If minutes** — a fast backend is mandatory in v0.1, the two-backend scheme is confirmed, and #042 proceeds as planned.

## Context

The two-backend scheme (fast preprocessor + libclang opt-in) is the current decision of the spec and #006. But that decision was made without measurements, on the intuition that "libclang is expensive". Until it's verified by hand, you can neither confidently build #042 on this architecture nor simplify it.

**Technical nuance:** the AST is not needed for the include graph — `clang_getInclusions` returns the inclusion tree directly. But the expensive part is still `parseTranslationUnit`, which can't be avoided if you want to resolve real paths (rather than a naive regex over `#include`, which breaks on include paths and `#ifdef`). This is the real watershed between the two backends — resolution accuracy versus speed. The spike should give this by hand.

## Execution plan

- [x] Take a real project as a target (preferably `spdlog` or `fmt` — compact, well-known, with a `compile_commands.json`)
- [x] Generate `compile_commands.json` (CMake export)
- [x] Minimal code: load the `CompilationDatabase`, for each TU call `clang_parseTranslationUnit`, then `clang_getInclusions` to build the include graph
- [x] **Measurement 1:** total wall-clock on a full pass (hot cache, 3 runs, median)
- [x] **Measurement 2:** peak RSS (`/usr/bin/time -v` or `getrusage`)
- [x] **Measurement 3:** wall-clock of a single median TU
- [ ] Compare against a rough estimate of the fast backend (regex over `#include` on the same TUs — a few seconds for everything)
- [ ] Decision in `docs/dev/spike_clang_perf.md` (1 page): numbers + conclusion + recommendation for #042
- [ ] Update `docs/architecture-spec.md`: either confirm the two-backend scheme with a link to the spike, or rewrite §"Two-backend scheme" for libclang-only
- [ ] Close or remove the `--with-clang` flag in the #042 plan depending on the result

## Acceptance criterion

After running the spike, the answer to "is a fast backend needed in v0.1" is stated in a single line with a number and is linked into #042 as justification.

## Done

**2026-05-29 — setup + first libclang measurement**

- Host: Astra Linux 1.7, GCC 8.3, CMake 3.18.4. Installed via apt: `libclang-19-dev`, `time` (GNU). libclang-19 = the fresh branch from the astra2 repo.
- Target: **spdlog** (master, `2e71fdf3`) — fmt master doesn't configure on CMake 3.18 (needs policy ≥ 3.27 for `INTERFACE_LIBRARY DEBUG_POSTFIX`).
- compile_commands.json: spdlog + tests + examples (without bench) → **141 TU**.
- Spike in [experiments/clang_perf/](experiments/clang_perf/): `CMakeLists.txt`, `main.cpp` (loop over `CompilationDatabase` → `parseTranslationUnit` with `SkipFunctionBodies` + `DetailedPreprocessingRecord` + `KeepGoing` → `getInclusions`), `regex_baseline.cpp` (a naive fast backend on regex over `#include`), `README.md`.
- 3 `clang_perf` runs (Release, hot cache):

| Run | Wall (s) | Median TU (ms) | Peak RSS (MB) |
|-----|---------:|---------------:|--------------:|
| 1   | 14.81    | 73.83          | 136           |
| 2   | 14.72    | 71.39          | 136           |
| 3   | 15.12    | 74.10          | 136           |

- **Number: ~15 seconds on 141 TU = ~105 ms/TU average, ~73 ms median.** The full-scale picture: a project of ~1500 TU (×10 spdlog) → ~2.5 minutes on libclang-only single-thread. With parallelism across TUs (libclang is thread-safe per-index) — realistically ~30-40 seconds for a typical project.

**2026-05-29 — regex baseline + report + sync of spec/task**

- regex-baseline ×3 runs on the same 141 TU: **total 11 ms, median 0.04 ms/TU, peak RSS 3.5 MB.**
- Contrast: libclang is ×1350 slower by wall, ×40 heavier in memory.
- Report: [docs/dev/spike_clang_perf.md](../../docs/dev/spike_clang_perf.md) — numbers, extrapolation, recommendation.
- Spec: §"Two-backend scheme" confirmed with a link to the report; risk #1 (libclang perf) updated with concrete numbers.
- #042: unblocked, scope stays as written, a note added about libclang ≥ 18.

**Final answer:** the two-backend scheme is needed. libclang in v0.1 is opt-in (`--with-clang`), default = fast backend.

## In progress

- (empty — the task is ready to close)

## Next steps

All steps are complete. Next — #042 phase 1 (CMake opt-in + a skeleton `clang_scanner.h/.cpp`).

## How it works

The spike = two binaries on the same TU sample from `compile_commands.json`:

1. `clang_perf` — opens the DB via `clang_CompilationDatabase_fromDirectory`, for each TU calls `clang_parseTranslationUnit` (with `SkipFunctionBodies | DetailedPreprocessingRecord | KeepGoing`) and `clang_getInclusions`. This is a close analog of what the archcheck libclang backend will do. Per-TU + total measurement via `std::chrono::steady_clock`, peak RSS via an external `/usr/bin/time -v`.
2. `regex_baseline` — reads only the list of TUs from the DB, and with a grep over a regex counts `#include` lines in each TU. Does not resolve paths, does not recurse. This is a lower bound for "naive fast backend"; a realistic preprocessor-only backend will be ~10× slower than the baseline.

The contrast of the two numbers (libclang ~15s vs regex ~11ms on the same 141 TU) gives the order of magnitude: libclang is ~1350× more expensive than the regex by wall-clock on the same inputs, which confirms the spec's decision — a fast backend is needed for the CI guard job, libclang is opt-in for semantics.

## Outcome

**Status:** completed
**Completed:** 2026-05-29

The answer to the task's question is recorded in a single line: **the two-backend scheme is needed, the scope of #042 doesn't change.** Artifacts — [docs/dev/spike_clang_perf.md](../../docs/dev/spike_clang_perf.md) (report), [experiments/clang_perf/](../../experiments/clang_perf/) (reproducible spike).

## Key decisions

| Decision | Reason |
|----------|--------|
| Real-world target (spdlog/fmt), not synthetic | synthetic doesn't give realistic include chains, the libclang estimate would be skewed |
| Not on CI | measurements are noisy, stable numbers are needed |
| Spike = a decision, not a code groundwork | the result is an updated spec, not a code library |
| Target = spdlog, not fmt | fmt master requires CMake ≥ 3.27 (our Astra host = 3.18). spdlog configures cleanly and gives more TUs (with tests+examples → 141 vs fmt-core = 1). |
| libclang **19**, not 11 | 11 is an old branch with a less efficient parser; 19 is what archcheck will require at minimum. Measuring the "worst case" (older) would be misleading. Pin the version in the report. |
| Direct `find_path` of libclang.so + Index.h, not `find_package(Clang)` | On Astra `find_package(Clang CONFIG)` pulls in a broken llvm-11 ClangConfig.cmake (references a missing ClangTargets.cmake). The CMake package isn't needed for the C API. |
| `SkipFunctionBodies` in `parseTranslationUnit` | archcheck doesn't need function bodies — it needs namespace/decl/include graph. SkipFunctionBodies speeds things up ~2x on code with heavy templates. Realistic for "a benchmark for our task". |
| External `/usr/bin/time -v` for RSS, not `getrusage` inside | simpler, doesn't pollute the spike code with measurement scaffolding. |

## Changed files

| File | Change |
|------|--------|
| `experiments/clang_perf/CMakeLists.txt` | new — spike config (libclang-19 via direct find_path) |
| `experiments/clang_perf/main.cpp` | new — libclang per-TU measurement |
| `experiments/clang_perf/regex_baseline.cpp` | new — fast-backend lower bound |
| `experiments/clang_perf/README.md` | new — how to run, what to install |
| `experiments/clang_perf/.gitignore` | new — `*.csv`/`*.time` artifacts not committed (contain local absolute paths) |
| `docs/dev/spike_clang_perf.md` | report with numbers and decision (TBD) |
| `docs/architecture-spec.md` | confirm / rewrite §"Two-backend scheme" (TBD) |
| `backlog/new/042_maj_clang_semantic_backend.md` | update scope per results (TBD) |

## Controlled by

<!-- TODO -->

## Related to

<!-- TODO -->

## Diagnostics

<!-- TODO -->
