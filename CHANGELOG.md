# Changelog

All notable changes to archcheck are documented here.

The format follows [Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/) and versioning follows [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **CMake project skeleton.** C++20, CMake 3.18+, Ninja generator, FetchContent for `ryml` v0.7.0 and `Catch2` v3.7.1 — no system installs required. `archcheck` binary that supports `--version` / `--help` (exit code 2 on unknown argument). Smoke test suite. `.clang-format` (Allman / 3-space) and `.clang-tidy` (narrow starter set). Build verified on Astra Linux 1.7. (#004)
- Project foundation: process documentation, backlog with numbered tasks (`NNN_<priority>_<name>.md`), research catalog of 25 candidate architecture rules with attribution to C++ Core Guidelines, Lakos/BDE, Martin, Google, HIC++.
- Git workflow standards: GitHub Flow, Conventional Commits 1.0, SemVer 2.0, Keep a Changelog 1.1, annotated `vX.Y.Z` tags. See [`docs/dev/git_workflow.md`](docs/dev/git_workflow.md). (#007)
- `CHANGELOG.md` itself, following Keep a Changelog 1.1.
- Stability contract section in [`docs/architecture-spec.md`](docs/architecture-spec.md) defining what counts as a breaking change after v1.0. (#007)
- Architecture spec: two-backend decision recorded — fast preprocessor-only backend is the v0.1 default, libclang opt-in via `--with-clang` (lands as v0.2). Semantic SF rules (SF.2/5/10/11) moved from v0.1 to v0.2. (#006)
- `README.md`: Secondary goal note — side experiment to test whether a useful CLI tool can be built end-to-end purely through agent conversation.

### Changed

- **Product name locked to `archcheck`.** README, spec, and all internal docs now use `archcheck` consistently (previously split between `cpparch` and `archcheck`). Name availability verified on GitHub, PyPI, crates.io, Homebrew, and npm — all clear. Local working directory remains `cpparch` for tool-path stability. (#003)
- **Architecture spec refactored to v2.1** (#006):
  - Headline repositioned around "module boundaries + cycles in CI" with `--baseline` day-one; Lakos / Core Guidelines / Martin demoted from brand to cited sources.
  - AI-guardrail angle (EURECOM constraint decay paper) lifted into TL;DR.
  - Roadmap rewritten: v0.1 = fast preprocessor backend without `compile_commands.json`; libclang opt-in / v0.2; Martin metrics v0.4 opt-in; `--suggest-config` killed.
  - Default-analysis section: SF rules now have a Phase column (v0.1 / v0.2); SF.4 dropped from defaults with rationale.
  - Config section reoriented: `modules` + `forbidden_deps` are the headline, defaults section secondary; "minimal config" example for legacy projects added.
  - Risks audited: license risk removed (resolved); libclang/`compile_commands.json` risks marked v0.2+; Martin's-A risk marked v0.4; templates risk clarified by phase.
  - "Следующие шаги" trimmed of items already done.

[Unreleased]: https://github.com/blurman-ai/archcheck/commits/master
