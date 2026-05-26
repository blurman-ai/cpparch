# Changelog

All notable changes to cpparch are documented here.

The format follows [Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/) and versioning follows [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project foundation: process documentation, backlog with numbered tasks (`NNN_<priority>_<name>.md`), research catalog of 25 candidate architecture rules with attribution to C++ Core Guidelines, Lakos/BDE, Martin, Google, HIC++.
- Git workflow standards: GitHub Flow, Conventional Commits 1.0, SemVer 2.0, Keep a Changelog 1.1, annotated `vX.Y.Z` tags. See [`docs/dev/git_workflow.md`](docs/dev/git_workflow.md). (#007)
- `CHANGELOG.md` itself, following Keep a Changelog 1.1.
- Stability contract section in [`docs/architecture-spec.md`](docs/architecture-spec.md) defining what counts as a breaking change after v1.0. (#007)
- Architecture spec: two-backend decision recorded — fast preprocessor-only backend is the v0.1 default, libclang opt-in via `--with-clang` (lands as v0.2). Semantic SF rules (SF.2/5/10/11) moved from v0.1 to v0.2. (#006)
- `README.md`: Secondary goal note — side experiment to test whether a useful CLI tool can be built end-to-end purely through agent conversation.

[Unreleased]: https://github.com/blurman-ai/cpparch/commits/master
