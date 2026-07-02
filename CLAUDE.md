# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Be warm and personal; emotional support is welcome.

## Project status

**v0.1 in active development.** The core pipeline (fast preprocessor scan → include graph → default rules → reporters) is implemented and builds; the binary ships SF.7/8/9 + Lakos rules, baseline, drift, and diff modes. For what has actually shipped, [CHANGELOG.md](CHANGELOG.md) is authoritative — don't restate version-specific scope here, it drifts.

Product name is locked to `archcheck` (binary `archcheck`); availability on GitHub/PyPI/crates.io/Homebrew/npm was verified clear (#003). The local working directory stays `cpparch` for tool-path stability.

## What this tool is

CI-first CLI that enforces architectural invariants on C++ projects. Scans sources with a fast preprocessor pass (no `compile_commands.json` required — the libclang backend that reads it is v0.2), builds the include dependency graph, applies a set of authority-backed default rules (YAML module rules are parsed and validated; enforcement lands v0.2), reports violations as `file:line`, exits non-zero on violations.

Positioning is deliberately **not** "ArchUnit for C++". It is "Lakos physical design + C++ Core Guidelines SF.* checks in CI" — speaking the native dialect of the C++ community. Every default rule carries attribution (Core Guidelines, Lakos, Martin) so users cannot dismiss it as opinion. Preserve this framing in docs, error messages, and marketing copy.

## What it explicitly is NOT

- Not a linter (clang-tidy's job)
- Not a bug finder (PVS-Studio, Coverity, cppcheck)
- Not a formatter (clang-format)
- Not an include optimizer (IWYU)
- Not a GUI, not a web dashboard, not a VS Code extension — CLI and CI only

When tempted to add a feature, check it against this list first.

## Planned architecture (from spec)

```
parser → graph → rules → reporter
```

Pipeline modules (planned layout under `src/`):

- `config/` — YAML loader → internal `Config` struct
- `scan/` — two backends: `include_scanner` (fast, preprocessor-only, no `compile_commands.json` needed) and `clang_scanner` (libclang, for semantic rules)
- `graph/` — component DAG, cycle detection, levelization, CCD/ACD/NCCD metrics
- `rules/` — one rule per class implementing `IRule`; the actual directory is flat (`src/rules/*.cpp`), the grouping by source (core_guidelines/lakos/martin) never materialized
- `report/` — `text_reporter`, `json_reporter` (`sarif_reporter` — planned)
- already present beyond the plan: `diff/` (regression report for `--diff`), `git/` (fork/exec git, reading blobs), `scan/duplication/` (token-based clone detector, advisory)

**Two-backend split is a deliberate design choice** — most useful default rules (SF.7/8/9/21, cycles, god-headers, chain length) are include-only and shouldn't pay the libclang cost. Don't collapse the backends without explicit discussion. The final decision is flagged in the spec as "deferred to a v0.1 spike."

**One rule = one class = one file.** Registration happens in the factory in `src/rules/rule_set.cpp` (`makeDefaultRuleSet` / `makeDriftRuleSet`): a new rule = a new pair of files + one line in the factory, existing rule files are left untouched. Don't refactor toward a "generic rule engine" that violates this.

## Tech stack (planned)

- **C++20**, CMake, libclang/libtooling, YAML via `ryml` or `yaml-cpp`, Catch2 or GoogleTest, GitHub Actions.
- **Dependencies: minimum.** No Boost. No heavy graph libraries — `unordered_map<NodeId, vector<NodeId>>` is sufficient.
- **Distribution: single static binary** per platform (Linux x86_64/arm64, macOS arm64, Windows x64), plus Docker image. Runtime depends only on libclang, statically linked.

## Exit codes (contract — don't change without versioning)

- `0` — OK
- `1` — violations found
- `2` — config / parsing error
- `3` — internal error

## Default rules

Defaults are deliberately a conservative subset to avoid the "5000 violations on first run" failure mode, and `--baseline` exists from day one so legacy projects can adopt without rewriting. The shipped rule list and thresholds live in [CHANGELOG.md](CHANGELOG.md) (authoritative) and `archcheck --help`; the staged rule roadmap lives in [docs/architecture-spec.md](docs/architecture-spec.md) §Roadmap. Don't restate either list here — it drifts.

## Fixtures are mandatory

From [docs/MVP.md](docs/MVP.md): *"If feature cannot be tested with fixtures — do not implement it."* Every rule needs a `fixtures/<rule>/pass/` and a `fixtures/<rule>/fail_*/` directory. This is the acceptance bar, not a nice-to-have.

## Design docs — read these before non-trivial work

- [docs/architecture-spec.md](docs/architecture-spec.md) — full spec v2.2, ~760 lines. Authoritative source for rule lists, rule attribution, roadmap (v0.1 → v0.5), risks, target audience, and the rationale behind every architectural choice. When in doubt about scope or framing, this doc wins over README.
- [docs/MVP.md](docs/MVP.md) — MVP scope and acceptance criteria. Shorter, English. Use this to decide whether something belongs in v0.1.
- [docs/decisions/](docs/decisions/) — ADR: accepted deferral decisions (config→v0.2, SF.21→v0.2, fast backend default). Check here BEFORE declaring a missing feature a "gap" — it may be a decision, not debt.
- [README.md](README.md) — public-facing pitch; example config and CLI shape live here.
- [docs/research/constraint_decay.md](docs/research/constraint_decay.md) — the project's root cause: a retelling of the Dente et al. (EURECOM, 2026) article on constraint decay and an analysis of the HN discussion. Read it when you need to recall *why*.
- [docs/dev/coverage_constraints.md](docs/dev/coverage_constraints.md) — why branch coverage is stuck at ~63% and what to do about it: the lcov 1.13 limitation on Astra Linux 1.7 (GCC 8.x doesn't mark throw-arcs in a way lcov can drop them). The branches threshold is deliberately left at 40%.
- [docs/duplication_architecture.md](docs/duplication_architecture.md) — the single source of truth for the duplicate-detection subsystem: complementary layers (#053 line / #056 token / #052 AST / #059 precision / #054 usage), the token-pass pipeline, selective normalization, metrics and their semantics, FP classes, modes, boundaries. Read it before working on any duplication layer.
- [docs/dev/haiku_task_guide.md](docs/dev/haiku_task_guide.md) — a two-sided contract for tasks executed by Haiku: the author's checklist (Haiku-ready task) and the executor's rules (what to watch for / what to never do). Read it on both sides: whoever writes the task in `backlog/`, and Haiku before starting.

## Working principles from the spec

- **YAGNI.** Don't build a feature until a concrete user asks. The roadmap is intentionally staged.
- **Authority over opinion.** Every default rule must cite Core Guidelines / Lakos / Martin. If a proposed default rule has no citation, it goes under Level 4 ("indisputable practices") or doesn't ship as a default.
- **Zero-config first.** Running with no arguments must produce a useful result.
- **Deterministic.** Same input → same output. Required for CI use.
- **Boring tech.** C++20, libclang, YAML, GitHub Actions. Don't reach for novel dependencies.

## Self-checking conclusions (mandatory)

A recurring pattern that needs to be broken: a run produces a result → the conclusion "it works correctly" is drawn → the result is shown to the user → the user says "this looks suspicious, double-check" → the re-check confirms the conclusion was unfounded. To stop kicking this ball back and forth:

- **The skeptic's role is yours, before showing the result.** Before concluding "the result is correct / the scanner works", ask yourself the questions the user would ask: are the numbers plausible? isn't it too smooth? what would look the same if the logic were broken? Check 2–3 concrete cases by hand (input → expected output), not just the final summary.
- **The mere fact that a run finished and produced numbers is not proof of correctness.** A "correct" conclusion must rest on an independent cross-check, not on the internal template "the run passed → OK".
- **If you catch yourself in an unfounded conclusion, re-check before sending**, not after the user objects.
- **If uncertainty remains, communicate the doubts explicitly**: what exactly was not verified and what looks suspicious.
- **Don't close a task until you are fully confident the results are sound.** Doubts remain → the task stays open, and the doubts go to the user.

## Journey log — JOURNEY.md (mandatory to maintain)

`~/projects/archcheck-journal/JOURNEY.md` — a living human-in-the-loop log of **findings, dead ends, and reversals**
(the body of a future article). It lives in the PRIVATE companion repo
[blurman-ai/archcheck-journal](https://github.com/blurman-ai/archcheck-journal) (moved out of the
public repo 2026-07-02, task #167, together with `milestones.md`, `dup_band_70_80.md`, `analysis/`);
after appending, commit+push in that repo — parallel agents rely on git as the collision safety net.
**Rule: append every key finding and significant episode to JOURNEY while
the trail is hot** — don't postpone, don't "we'll recall it later from git log" (git log doesn't store this). Append
whenever any of the following happens:
- **a key finding / result** — a corpus-run conclusion, a confirmed/refuted signal, a number
  with a verdict (what was proved or disproved, and what killed the signal);
- a non-trivial success (found the cause, a method reversal, a breakthrough);
- **a dead end / wrong conclusion** that was caught (especially if the author caught it with a remark);
- a moment where the author's steering turned the work around.

Format: date + context + the agent's move/hypothesis + the author's **verbatim** remark (if there was a reversal;
optional for a finding-result) + analysis + lesson/takeaway + commit. Write honestly, including (and especially)
about the agent's mistakes and null results — that's where the value is. Don't confuse this with memory: memory = an atomic
recall fact; JOURNEY = a narrative. Major evidential results also live in
[docs/research/central_finding.md](docs/research/central_finding.md); in JOURNEY they appear as a milestone of the path.

## Code style & AI constraints

These apply to all C++ code in this repo. Before your first non-trivial code change, read both files in full:

- [docs/code_style.md](docs/code_style.md) — C++20 style guide (Allman, 2 spaces, naming, NVI, C++20 features, tooling).
- [docs/code_quality.md](docs/code_quality.md) — anti-AI-slop rules, thresholds (functions ≤ 30, classes ≤ 300, ≤ 50 new lines per commit, ≤ 2 new files, 0 abstractions without a request), forbidden patterns, self-check before committing.
- [docs/dev/git_workflow.md](docs/dev/git_workflow.md) — git process: GitHub Flow, Conventional Commits, SemVer 2.0, Keep a Changelog, annotated `vX.Y.Z` tags.

Archcheck is itself an architecture-checking tool, so **dogfooding is mandatory**: archcheck's code must pass its own rules. There is no self-check gate in CI yet (there's smoke + diff mode in the PR workflow; adding a self-check step is an open task), so run `./build/debug/src/archcheck` from the root locally: `src/`, `include/`, `tests/` must produce 0 violations. Any merge that breaks the tool's own rules is unacceptable.

## Tasks & workflow

Tasks live in `backlog/`, one `.md` per task. Completed ones move to `backlog/completed/` with appended sections about how they work — this builds the system's documentation between sessions. See [backlog/README.md](backlog/README.md).

Slash commands (`.claude/commands/`):
- **`/create-task <name>`** — a new task in `backlog/`.
- **`/checkpoint`** — update the active task with current progress.
- **`/backlog-review`** — a report on stale / quick-win / blocked tasks.
- **`/commit`** — assemble a commit with AI trailers (`AI-Assisted`, `Verified`, `Risk`, `Co-Authored-By`), shown to the user before creation.
- **`/findings`** — at the end of a session: pull out of the conversation what's worth putting into memory (new / reinforcing existing / confirmed moves), show the list, update only with confirmation.

**Never commit without an explicit command** (`/commit` or "make a commit"). When you finish a task — wait.

**Run builds freely** — this is a tool project, and verifying a change through a real compile is the norm here. By default build **Debug**, not Release. After building, fill in the `Verified:` trailer in the commit (`build`, `build+tests`, `manual`).

## Build / test / run

C++20, CMake 3.18+, the Ninja generator, FetchContent for dependencies (ryml + Catch2). The first build downloads dependencies into `build/_deps/` (internet required); subsequent builds are offline.

```bash
# Debug configuration — the default.
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build:
cmake --build build/debug

# Run tests:
cd build/debug && ctest --output-on-failure
# or directly:
./build/debug/tests/archcheck_tests

# Run the binary:
./build/debug/src/archcheck --version
./build/debug/src/archcheck --help
```

Layout (see also `docs/architecture-spec.md` §"High-level code structure"):
- `src/` — entry point + subsystems (config/, scan/, graph/, rules/, report/ appear as v0.1 progresses).
- `include/archcheck/` — public headers.
- `tests/` — Catch2-based unit/integration tests.
- `fixtures/` — sample projects with known violations (for rule tests, appearing alongside the rules).

Dependencies are `ryml` (YAML) and `Catch2` v3 (tests only). Pulled in via FetchContent; for offline CI, `build/_deps/` is cached via `actions/cache` in #002.

Tooling: `.clang-format` (Allman, 2 spaces, 120 columns), `.clang-tidy` (bugprone-/performance-/modernize-/cppcoreguidelines-/readability- with noisy checks disabled).
