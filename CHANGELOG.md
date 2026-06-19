# Changelog

All notable changes to archcheck are documented here.

The format follows [Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/) and versioning follows [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **`--diff` now surfaces copy-paste a commit introduces** — when a change adds new code,
  `--diff` reports duplicate blocks that the change itself brought in (a block copied from
  code already in the tree, or two identical blocks added together), not just a snapshot of
  the whole project. Advisory only — it never fails the build; the blocking gate stays v0.2.
  Entry point lands in `--diff`; corpus precision measurement is tracked in #103. (#123)
- **`diff_max_added_lines` — bulk imports skip the complexity advisory** — a change that
  adds more than the configured number of lines (a vendored drop, generated code, a large
  paste) no longer triggers the per-function complexity advisory (`DRIFT.LOCAL_COMPLEXITY`),
  which would otherwise flag every imported function. (#117)
- **DRIFT.4 `lateral_module_dependency`** — detects the first lateral dependency between
  peer modules (neither previously depended on the other in baseline). Three graded
  sub-rules: `DRIFT.4.CYCLE` (creates module-level cycle with a baseline back-edge, gates
  in `--drift` mode alongside DRIFT.1/2), `DRIFT.4.SDP` (violates Martin Stable
  Dependencies: I(B) > I(A) + 0.10, advisory), `DRIFT.4.NEW` (first lateral pair, advisory).
  Mass-touch guard: >150 added edges → rule is silent (reorg/vendor drop). Shared layer
  (FID > 0.5·maxFID AND FOD ≤ medianFOD) is also silent. Logic extracted from corpus
  validation in #111/#115/#117 (417-event baseline on 479 repos, CYCLE precision 92%).
  `areaOf` extracted into shared `include/archcheck/rules/area_of.h` (was inline in
  DRIFT.3). Architecture spec bumped to v2.3 (DRIFT.4–9 wave → DRIFT.5–10). (#118)

- **JSON output for `--diff`** — `--diff --format=json <revspec>` emits one stable JSON
  document (schema `version: 1`): refs, `gate: ok|fail`, a `gating` block (grown cycles,
  new god-headers) and an `advisory` block (added/removed edges, cross-area dependencies,
  chain-length/NCCD deltas, SATD / test co-evolution / complexity-drift violations).
  Covered by product-level E2E tests that run the real binary on synthesized repos
  (clean diff / new edge / new cycle / docs-only fast path). (#075)

- **SATD delta advisory in `--diff`** — added lines of a diff are scanned for self-admitted
  technical debt markers in comments: `SATD.1` (TODO/FIXME/HACK/XXX/TEMP, plus
  temporary/workaround/quick fix/dirty) and `SATD.2` (FIXME/HACK without an issue id).
  Reported after the structural diff, never gates. Shared `git_exec` (fork/exec git helper,
  extracted from git_state) and `diff_query` (added-lines / numstat parsers) land as
  reusable infrastructure. (#096)
- **Test co-evolution advisory in `--diff`** — `TEST.1.prod_changed_tests_silent` flags a
  diff with significant production churn and silent tests (prod ≥ 80 lines with tests = 0,
  or prod ≥ 200 with test/prod ratio < 5%). Advisory-only. (#097)
- **`--history <path>` advisory mode** — repository-history analytics over one
  `git log --numstat` pass (shared `history_query` parser): `SIZE.1.god_file_growth`
  flags files that are already large (≥ P75), grew ≥ +30% or +300 lines, in ≥ 5
  consecutive growing commits with no meaningful shrink (#098); `HIST.1.defect_attractor`
  flags production files in the top decile of fix-like commits (≥ 5 touches), skipping
  merges and mechanical (>30-file) commits. Always exits 0. (#100)
- **decision records** — `docs/decisions/` (ADR-001 config-rules→v0.2, ADR-002 SF.21→v0.2,
  ADR-003 fast-backend-default), surfacing deferral decisions previously buried in
  `backlog/completed/`. `docs/MVP.md` rewritten around zero-config acceptance criteria. (#045)
- **Local complexity drift advisory in `--diff`** — per-function Sonar Cognitive Complexity
  (Campbell 2018, token-level, no AST) compared between baseline and current versions of
  the changed C/C++ files. `DRIFT.LOCAL_COMPLEXITY` reports a function that crossed the
  absolute threshold 25 (Sonar C-family / clang-tidy default), grew by ≥ 3 while already
  above it, or grew by ≥ 5 in one diff; volume is not complexity — adding flat statements
  scores 0. Formula validated on a 100-repo corpus (#102: 13/13 synthetic, 6/6 manual TP).
  Advisory-only, never gates. (#101)

### Security

- **Hardened against untrusted repositories** (CI threat model): file-tree walk no longer
  follows symlinks pointing outside the scan root (S3); file and git-blob reads are capped
  at 64 MiB and skipped with a diagnostic instead of risking OOM (S4); `jsonEscape` now
  emits RFC 8259-valid output for all control characters and invalid UTF-8 (S5); all child
  `git` invocations run with `GIT_CONFIG_NOSYSTEM=1`, `core.hooksPath=/dev/null`,
  `core.fsmonitor=`, `core.pager=cat` and `--no-ext-diff`, so a malicious `.git/config`
  or hook cannot execute commands (S6). (#105)

### Changed

- **`--diff` is advisory-first** — exit 1 now means only a gated regression: a new/grown
  include cycle (SF.9-class) or a new god-header crossing the fan-in threshold
  (Lakos.GodHeader-class). Added/removed edges, new cross-area dependencies, chain-length
  and NCCD growth are still reported but marked `(advisory)` and no longer fail the run —
  a PR that merely adds an `#include` stays green. The text report ends with an explicit
  `gate: ok|fail` verdict line. Mirrors the `--drift-baseline` regression-gate semantics (#086). (#075)
- **`check` mode is advisory-first too** — a plain `archcheck <repo>` now exits 1 only on a
  dependency cycle (SF.9). Deep include chains (Lakos.ChainLength), god-headers
  (Lakos.GodHeader) and per-file hygiene (SF.7/SF.8) are still reported but advisory (exit 0),
  with a note pointing to `--baseline` for existing debt. A naive first run on a header-heavy
  library (abseil: 211 chain-length findings) no longer fails with a wall of noise — the gate
  fires only on the architectural regression the tool is most precise on. Mirrors the `--diff`
  gating model above. (#133)
- **`git` execution unified** — the fork/exec git helper is shared across `git_state` and
  `git_object_file_source` via `git/git_exec` (removes ~50 lines of duplication). (#096, #105)

### Removed

- **Dead code surfaced by the 2026-06 audit** (~330 lines, 14 test cases): the unused
  `diffTokens`/`DiffOp` LCS machinery in the clone classifier; test-only graph helpers
  `reverseReachableFrom`/`hasPath`; the placeholder `evaluateAgainstCorpus` (always
  precision 1.0); and several write-only/unread fields and accessors
  (`MetricThresholds::chainLengthLimit`, `Pair::sharedRare`, `BaselineLoadError::line`,
  `ConfigError` file/line/column accessors, `Worktree::valid()`, `DiskFileSource::root()`). (#104)

- **Scale-independent duplication candidate generation** — k-gram winnowing fingerprints (MOSS-style) added alongside the rare-token index. The rare-token index keyed on corpus document-frequency, so a genuine clone pair stopped being a candidate once the project grew enough that its shared tokens were no longer "rare" — detection depended on project size. Fingerprints are intrinsic to each fragment's token run, so a clone is a candidate at any corpus size; over-frequent fingerprints (boilerplate idioms) are dropped to bound cost. Recovers function-level clones the index was hiding (verified eyes-on, ~0 added false positives). (#092)

- **SF.7 rule** — no `using namespace` at global scope in headers, with block-comment stripping and brace-depth tracking. (#034, #035, #038)
- **SF.8 rule** — header self-sufficiency, with Objective-C exclusion and scan-limit handling. (#034, #039)
- **SF.9 rule** — no cyclic dependencies among headers.
- **Lakos.GodHeader rule** — fan-in threshold (default 50). (#037)
- **Lakos.ChainLength rule** — include chain length (default 10).
- **DRIFT.1 / DRIFT.2 rules** — shortcut edges and cycle growth against a saved graph baseline. (#009, #040)
- **DRIFT.3 rule** — new bidirectional module coupling: fires when two modules (areas) become mutually dependent (A→B and B→A through different files) without having been mutual in the baseline, i.e. a non-levelizable aggregate-level coupling that no file-level cycle (DRIFT.2/SF.9) catches. Backed by corpus validation ([docs/research/drift_signal_validation.md](docs/research/drift_signal_validation.md)) and a live run on a real repo. (#087)
- **Drift gate semantics** — `--drift-baseline` is now a regression gate: only DRIFT.1 (new shortcut edge) and DRIFT.2 (new/grown cycle) fail the run; pre-existing intrinsic findings (SF.*/Lakos.*) and the advisory DRIFT.3 coupling signal are reported but no longer gate. A legacy repo with no regression in the diff now exits `0` instead of failing on its existing debt. (#086)
- **Baseline modes** — `--baseline`, `--save-baseline`, `--save-graph-baseline`, `--drift-baseline`.
- **PR diff mode** — `--diff <revspec>` reports structural graph regressions (added/removed edges, grown cycles, new god-headers, chain-length growth, new cross-area dependencies) between two git refs. (#076)
- **JSON reporter** and stabilised exit-code contract (`0` ok / `1` violations / `2` config error / `3` internal error).
- **Fast preprocessor backend** — runs without `compile_commands.json` and without libclang; default for v0.1.
- **PR sticky-comment CI integration** — single auto-updating PR comment with violations. (#025)
- **Two-backend design confirmed** via libclang perf spike — fast backend stays default, `--with-clang` opt-in lands v0.2. (#043)
- **Config loader v1 — phase 1+2** — parses `version` / `modules` / `rules` (with `layers` / `independence` / `forbidden` types), rejects unknown top-level keys, validates module existence and disjoint sets. Module rules are parsed and validated but not yet enforced (enforcement — v0.2). (#051)
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
- **`.clang-format` rebased on LLVM style** — indent migrated 3 → 2 spaces (the #004 entry below describes the original 3-space setup).
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

- **Malformed `.archcheck.yml` crashed with SIGABRT instead of exit 2** — ryml's abort()ing default error handler is now replaced with a throwing one for the whole config parse (same pattern as graph-baseline loading); malformed YAML reports `file:0:0: YAML error: …` and honours the exit-code contract.
- **Stack overflow on pathologically deep include chains** — SCC depth computation (runs inside the default Lakos.ChainLength rule) rewritten from native recursion to an explicit-stack DFS; guarded by a 200k-node regression test.
- **Nonexistent path silently passed with exit 0** — `archcheck <path>` and `--scan` / `--graph` / `--duplication` now exit 2 with `not a directory` instead of reporting "No violations found." on a missing directory.
- **Exit code 3 was unreachable** — `main` now has a top-level catch translating unhandled exceptions (including `bad_alloc`) into the documented internal-error code 3 instead of `std::terminate`.
- **CI smoke assertion was vacuous and dogfood was missing** — the smoke step now asserts exact exit codes (the old `cmd || test $? -eq 2` passed on any code), and a new CI step runs archcheck on its own `src/`, `include/`, `tests/` as a gate.
- **SF.9 false cycles on header + inline-impl pairs** — a two-node SCC formed by `x.h` ↔ `x-inl.h` / `.ipp` / `.tcc` (same directory, same stem) is recognised as an intentional inline split, not a cycle. (#088)
- **System `<...>` includes shadowed by project files** — angle-bracket system headers now resolve as External instead of matching a same-named project file, removing phantom edges and cycles. (#088)
- **Duplication recall on large functions** — fragmenter token cap raised 400 → 600 so large-function clones are no longer silently skipped. (#091)
- **In-tree bundled libraries classified as vendored** — bundled third-party sources inside the project tree are excluded from authored-code signals.
- **SF.9 silent on `#ifndef`-guarded cycles** — scanner now recognises `#ifndef`/`#define`/`#endif` include guards as unconditional includes. (#049)
- **UTF-8 BOM not stripped** at file start, causing scanner to miss the first directive. (#047)
- **Ambiguous includes resolved against mirror dirs** — skip well-known mirror trees (`copies/`, `upgrade/`, etc.) when picking a target. (#036)
- **Self-edge from system include suffix-collision** — a system/library `#include <name.h>` that suffix-matches a same-named project file (e.g. `src/compat/cpuid.h` → `<cpuid.h>`) no longer resolves to itself; tagged External/Unresolved instead. Removes phantom 1-node cycles in SF.9 (corpus: 26 false self-edges, 8 repos with a fake cycle). Single-candidate variant of #036. (#058)
- **GCC 8 / GCC 13 build warnings** — `starts_with` cppcheck noise silenced; unused-result warning in git object reader suppressed.
- **Relative include paths with `../` not resolved** — scanner now normalises `..`/`.` segments so relative includes resolve to the correct target instead of being dropped.
- **Duplication scanner over-excluded files** — the test/vendor exclusion no longer removes legitimate files from the duplication scan. (#081)

[Unreleased]: https://github.com/blurman-ai/cpparch/commits/master
