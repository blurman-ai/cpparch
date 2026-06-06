# Changelog

All notable changes to archcheck are documented here.

The format follows [Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/) and versioning follows [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **SF.7 rule** — single-statement-per-line with block-comment stripping and brace-depth tracking. (#034, #035, #038)
- **SF.8 rule** — header self-sufficiency, with Objective-C exclusion and scan-limit handling. (#034, #039)
- **SF.9 rule** — no cyclic dependencies among headers.
- **Lakos.GodHeader rule** — fan-in threshold (default 50). (#037)
- **Lakos.ChainLength rule** — include chain length (default 10).
- **DRIFT.1 / DRIFT.2 rules** — shortcut edges and cycle growth against a saved graph baseline. (#009, #040)
- **DRIFT.3 rule** — new bidirectional module coupling: fires when two modules (areas) become mutually dependent (A→B and B→A through different files) without having been mutual in the baseline, i.e. a non-levelizable aggregate-level coupling that no file-level cycle (DRIFT.2/SF.9) catches. Backed by corpus validation ([docs/research/drift_signal_validation.md](docs/research/drift_signal_validation.md)) and a live run on a real repo. (#087)
- **Baseline modes** — `--baseline`, `--save-baseline`, `--save-graph-baseline`, `--drift-baseline`.
- **PR diff mode** — `--diff <revspec>` reports structural graph regressions (added/removed edges, grown cycles, new god-headers, chain-length growth, new cross-area dependencies) between two git refs. (#076)
- **JSON reporter** and stabilised exit-code contract (`0` ok / `1` violations / `2` config error / `3` internal error).
- **Fast preprocessor backend** — runs without `compile_commands.json` and without libclang; default for v0.1.
- **PR sticky-comment CI integration** — single auto-updating PR comment with violations. (#025)
- **Two-backend design confirmed** via libclang perf spike — fast backend stays default, `--with-clang` opt-in lands v0.2. (#043)
- **Config loader v1 — phase 1+2** — parses `version` / `modules` / `rules` (with `layers` / `independence` / `forbidden` types), rejects unknown top-level keys, validates module existence and disjoint sets. (#051, in progress)
- **Config thresholds override** — `thresholds:` block parsed and threaded into the default rule set, enabling per-project override of god-header fan-in and chain-length limits. (#041)
- **Config format v1 (phase 1) specification** — `docs/config_format.md` defines `.archcheck.yml` v1: three top-level keys (`version` / `modules` / `rules`), three typed rule contracts (`layers`, `independence`, `forbidden`), four reference examples (tiny / layered / legacy / mixed), explicit phase-1/phase-2 scope table, and a SemVer contract for the schema itself (independent of binary version). Resolves the allowlist-vs-forbidden open question by rule type — `layers`/`independence` give implicit-allowlist strictness, `forbidden` stays as explicit blocklist for legacy adoption and surgical overrides. Loader implementation tracked separately as #051.
- **Static-analysis CI job** — parallel to build, runs cppcheck + lizard as gates (fail the build on findings) and clang-tidy strict as a warning-only baseline. Thresholds match `docs/code_quality.md` (CCN ≤ 15, function ≤ 30 lines, ≤ 5 args). Reports uploaded as 14-day artifact. (#001)
- **AI-assisted rule synthesis** — named in spec as the process where an AI agent reads the repository, builds an architectural hypothesis, and produces verifiable rules. Operational shape fixed: separate shell flow (`archcheck synthesize`), archcheck core never calls LLMs itself, output is a `.draft` config requiring explicit user confirmation. Paired conceptually with DRIFT rules into "synthesis + drift regression" loop. Formal contract deferred to #010. Risk entry added (false hypothesis / privacy / volatility).
- **CMake project skeleton.** C++20, CMake 3.18+, Ninja generator, FetchContent for `ryml` v0.7.0 and `Catch2` v3.7.1 — no system installs required. `archcheck` binary that supports `--version` / `--help` (exit code 2 on unknown argument). Smoke test suite. `.clang-format` (Allman / 3-space) and `.clang-tidy` (narrow starter set). Build verified on Astra Linux 1.7. (#004)
- Project foundation: process documentation, backlog with numbered tasks (`NNN_<priority>_<name>.md`), research catalog of 25 candidate architecture rules with attribution to C++ Core Guidelines, Lakos/BDE, Martin, Google, HIC++.
- Git workflow standards: GitHub Flow, Conventional Commits 1.0, SemVer 2.0, Keep a Changelog 1.1, annotated `vX.Y.Z` tags. See [`docs/dev/git_workflow.md`](docs/dev/git_workflow.md). (#007)
- `CHANGELOG.md` itself, following Keep a Changelog 1.1.
- Stability contract section in [`docs/architecture-spec.md`](docs/architecture-spec.md) defining what counts as a breaking change after v1.0. (#007)
- Architecture spec: two-backend decision recorded — fast preprocessor-only backend is the v0.1 default, libclang opt-in via `--with-clang` (lands as v0.2). Semantic SF rules (SF.2/5/10/11) moved from v0.1 to v0.2. (#006)
- `README.md`: Secondary goal note — side experiment to test whether a useful CLI tool can be built end-to-end purely through agent conversation.
- **Duplication detector FP guards (P0+P1)** — nine precision-improving filters:
  - **P0 Mechanical** (6 guards, low-risk, no AST): symmetric-pair canonicalization (P0.5), coordinate revalidation (P0.3), same-function filter (P0.1), function-boundary anchor (P0.4), git rename/move suppress (P0.2, simplified), joint token∧order floor (P0.6).
  - **P1 Classifiers** (3 guards, requires validation): data-table/literal-run classifier (P1.1), boilerplate-density filter (P1.2), file-local IDF down-weight (P1.4). (P1.3 header-impl gate shipped as a no-op placeholder and was removed in cleanup; full implementation planned — see #070.)
  - **measurement-harness** infrastructure for evaluating guards against `fp_corpus_r2.tsv` ground truth (CorpusMetrics, load/evaluate functions).
  - **Expected precision improvement**: baseline 42% → P0: ~55–62% → P1: ~65–75%. Idiom-floor ~40 FP unremovable without semantics (LLM confirmation planned v0.2).
- **Vendored-code exclusion** — both the include graph and the duplication scan now skip vendored single-file libraries and vendored directories outside `third_party/`, removing phantom signal from bundled dependencies. (#068, #069, #071)
- **Test-code exclusion** — test files are excluded from architecture and duplication signals by default, so the zero-config first run reflects production code. (#070)
- **Clone-type labels** — reported duplication pairs are tagged Type-1 / Type-2 / Type-3 (exact / renamed / gapped).
- **`--duplication` advisory report** — `archcheck --duplication <path>` scans for duplicate code and reports pairs (file:line ranges, clone type, weight). Shipped as a supported **advisory** capability: report-only, always exits `0`, and never gates CI — gate-grade precision is future work, so it is deliberately not a blocking check.

### Changed

- **God-header threshold raised** 30 → 50 to cut noise on real-world headers without hiding actual hubs. (#037)
- **README rewritten** to match the shipped v0.1 CLI surface. (#044)
- **Product name locked to `archcheck`.** README, spec, and all internal docs now use `archcheck` consistently (previously split between `cpparch` and `archcheck`). Name availability verified on GitHub, PyPI, crates.io, Homebrew, and npm — all clear. Local working directory remains `cpparch` for tool-path stability. (#003)
- **Architecture spec refactored to v2.1** (#006):
  - Headline repositioned around "module boundaries + cycles in CI" with `--baseline` day-one; Lakos / Core Guidelines / Martin demoted from brand to cited sources.
  - AI-guardrail angle (EURECOM constraint decay paper) lifted into TL;DR.
  - Roadmap rewritten: v0.1 = fast preprocessor backend without `compile_commands.json`; libclang opt-in / v0.2; Martin metrics v0.4 opt-in; `--suggest-config` killed.
  - Default-analysis section: SF rules now have a Phase column (v0.1 / v0.2); SF.4 dropped from defaults with rationale.
  - Config section reoriented: `modules` + `forbidden_deps` are the headline, defaults section secondary; "minimal config" example for legacy projects added.
  - Risks audited: license risk removed (resolved); libclang/`compile_commands.json` risks marked v0.2+; Martin's-A risk marked v0.4; templates risk clarified by phase.
  - "Следующие шаги" trimmed of items already done.

### Fixed

- **SF.9 silent on `#ifndef`-guarded cycles** — scanner now recognises `#ifndef`/`#define`/`#endif` include guards as unconditional includes. (#049)
- **UTF-8 BOM not stripped** at file start, causing scanner to miss the first directive. (#047)
- **Ambiguous includes resolved against mirror dirs** — skip well-known mirror trees (`copies/`, `upgrade/`, etc.) when picking a target. (#036)
- **Self-edge from system include suffix-collision** — a system/library `#include <name.h>` that suffix-matches a same-named project file (e.g. `src/compat/cpuid.h` → `<cpuid.h>`) no longer resolves to itself; tagged External/Unresolved instead. Removes phantom 1-node cycles in SF.9 (corpus: 26 false self-edges, 8 repos with a fake cycle). Single-candidate variant of #036. (#058)
- **GCC 8 / GCC 13 build warnings** — `starts_with` cppcheck noise silenced; unused-result warning in git object reader suppressed.
- **Relative include paths with `../` not resolved** — scanner now normalises `..`/`.` segments so relative includes resolve to the correct target instead of being dropped.
- **Duplication scanner over-excluded files** — the test/vendor exclusion no longer removes legitimate files from the duplication scan. (#081)

[Unreleased]: https://github.com/blurman-ai/archcheck/commits/master
