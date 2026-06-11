# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Отвечай по-русски. Тепло и по-человечески, эмоциональная поддержка приветствуется.

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
- `rules/` — one rule per class implementing `IRule`; фактический каталог плоский (`src/rules/*.cpp`), группировка по источникам (core_guidelines/lakos/martin) не материализовалась
- `report/` — `text_reporter`, `json_reporter` (`sarif_reporter` — план)
- фактически сверх плана уже есть: `diff/` (regression report для `--diff`), `git/` (fork/exec git, чтение блобов), `scan/duplication/` (токеновый клон-детектор, advisory)

**Two-backend split is a deliberate design choice** — most useful default rules (SF.7/8/9/21, cycles, god-headers, chain length) are include-only and shouldn't pay the libclang cost. Don't collapse the backends without explicit discussion. The final decision is flagged in the spec as "deferred to a v0.1 spike."

**One rule = one class = one file.** Регистрация — фабрика в `src/rules/rule_set.cpp` (`makeDefaultRuleSet` / `makeDriftRuleSet`): новое правило = новая пара файлов + одна строка в фабрике, существующие rule-файлы не трогаются. Don't refactor toward a "generic rule engine" that violates this.

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

- [docs/architecture-spec.md](docs/architecture-spec.md) — full spec v2.2, ~760 lines, **in Russian**. Authoritative source for rule lists, rule attribution, roadmap (v0.1 → v0.5), risks, target audience, and the rationale behind every architectural choice. When in doubt about scope or framing, this doc wins over README.
- [docs/MVP.md](docs/MVP.md) — MVP scope and acceptance criteria. Shorter, English. Use this to decide whether something belongs in v0.1.
- [docs/decisions/](docs/decisions/) — ADR: принятые deferral-решения (config→v0.2, SF.21→v0.2, fast backend default). Сверяйся сюда ПЕРЕД тем, как объявить отсутствующую фичу «гэпом» — возможно, это решение, а не долг.
- [README.md](README.md) — public-facing pitch; example config and CLI shape live here.
- [docs/research/constraint_decay.md](docs/research/constraint_decay.md) — первопричина проекта: пересказ статьи Dente et al. (EURECOM, 2026) о constraint decay и разбор HN-дискуссии. Читать, когда нужно вспомнить *зачем*.
- [docs/dev/coverage_constraints.md](docs/dev/coverage_constraints.md) — почему branch coverage застрял на ~63% и что с этим делать: ограничение lcov 1.13 на Astra Linux 1.7 (GCC 8.x не размечает throw-arcs так, чтобы lcov их убирал). Branches-порог намеренно оставлен на 40%.
- [docs/duplication_architecture.md](docs/duplication_architecture.md) — единый источник истины по подсистеме поиска дубликатов: комплементарные слои (#053 line / #056 token / #052 AST / #059 precision / #054 usage), конвейер токенового прохода, selective normalization, метрики и их семантика, классы FP, режимы, границы. Читать перед работой над любым duplication-слоем.

## Working principles from the spec

- **YAGNI.** Don't build a feature until a concrete user asks. The roadmap is intentionally staged.
- **Authority over opinion.** Every default rule must cite Core Guidelines / Lakos / Martin. If a proposed default rule has no citation, it goes under Level 4 ("несомненные практики") or doesn't ship as a default.
- **Zero-config first.** Running with no arguments must produce a useful result.
- **Deterministic.** Same input → same output. Required for CI use.
- **Boring tech.** C++20, libclang, YAML, GitHub Actions. Don't reach for novel dependencies.

## Code style & AI constraints

Применяются ко всему C++ коду этого репо. Перед первым нетривиальным изменением кода прочитать оба файла целиком:

- [docs/code_style.md](docs/code_style.md) — C++20 style guide (Allman, 2 spaces, naming, NVI, C++20 features, инструменты).
- [docs/code_quality.md](docs/code_quality.md) — anti-AI-slop правила, пороги (functions ≤ 30, classes ≤ 300, ≤ 50 новых строк на коммит, ≤ 2 новых файла, 0 абстракций без запроса), forbidden patterns, self-check перед коммитом.
- [docs/dev/git_workflow.md](docs/dev/git_workflow.md) — git-процесс: GitHub Flow, Conventional Commits, SemVer 2.0, Keep a Changelog, аннотированные `vX.Y.Z` теги.

Archcheck — сам инструмент проверки архитектуры, поэтому **dogfooding обязателен**: код archcheck обязан проходить собственные правила. Самопроверочного гейта в CI пока нет (там smoke + diff-режим в PR-workflow; добавление self-check шага — открытая задача), поэтому прогоняй `./build/debug/src/archcheck` из корня локально: `src/`, `include/`, `tests/` должны давать 0 нарушений. Любой merge, ломающий собственные правила, — недопустим.

## Tasks & workflow

Задачи живут в `backlog/`, по одному `.md` на задачу. Завершённые переезжают в `backlog/completed/` с дописанными секциями про принцип работы — это формирует документацию системы между сессиями. См. [backlog/README.md](backlog/README.md).

Slash-команды (`.claude/commands/`):
- **`/create-task <name>`** — новая задача в `backlog/`.
- **`/checkpoint`** — обновить активную задачу по текущему прогрессу.
- **`/backlog-review`** — отчёт по протухшим / быстрым победам / заблокированным.
- **`/commit`** — собрать коммит с AI-трейлерами (`AI-Assisted`, `Verified`, `Risk`, `Co-Authored-By`), показать пользователю до создания.
- **`/findings`** — в конце сессии: вытащить из разговора то, что стоит положить в memory (новое / усиление существующего / подтверждённые ходы), показать список, обновить только с подтверждением.

**Никогда не коммитить без явной команды** (`/commit` или «сделай коммит»). Завершил задачу — жди.

**Сборку запускать свободно** — это инструмент-проект, верификация изменения через реальную компиляцию здесь норма. По умолчанию собирать **Debug**, не Release. После сборки заполнять `Verified:` trailer в коммите (`build`, `build+tests`, `manual`).

## Build / test / run

C++20, CMake 3.18+, Ninja-генератор, FetchContent для зависимостей (ryml + Catch2). Первый билд скачивает зависимости в `build/_deps/` (нужен интернет); последующие — offline.

```bash
# Конфигурация Debug — по умолчанию.
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Сборка:
cmake --build build/debug

# Прогон тестов:
cd build/debug && ctest --output-on-failure
# или напрямую:
./build/debug/tests/archcheck_tests

# Запуск бинаря:
./build/debug/src/archcheck --version
./build/debug/src/archcheck --help
```

Layout (см. также `docs/architecture-spec.md` §«Высокоуровневая структура кода»):
- `src/` — entry point + subsystems (config/, scan/, graph/, rules/, report/ появляются по мере v0.1).
- `include/archcheck/` — публичные заголовки.
- `tests/` — Catch2-based unit/integration тесты.
- `fixtures/` — sample-проекты с известными нарушениями (для тестов правил, появятся вместе с правилами).

Зависимости — `ryml` (YAML) и `Catch2` v3 (только под тесты). Подтягиваются FetchContent-ом; для offline-CI кэшируется `build/_deps/` через `actions/cache` в #002.

Tooling: `.clang-format` (Allman, 2 пробела, 120 колонок), `.clang-tidy` (bugprone-/performance-/modernize-/cppcoreguidelines-/readability- с выключенными шумными чеками).
