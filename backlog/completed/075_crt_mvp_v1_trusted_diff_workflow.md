# [CLI][DIFF][REPORT] MVP v1: довести trusted PR/diff workflow до product-grade

**Дата создания:** 2026-06-02
**Дата старта:** 2026-06-12
**Статус:** wip
**Модуль:** CLI / DIFF / REPORT / DOCS
**Приоритет:** critical
**Сложность:** M
**Блокирует:** MVP v1 release readiness
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #023 (diff_skip_when_no_cpp_changes), #024 (in_memory_fs_for_diff), #025 (pr_comment_integration), #028 (rules_engine_mvp), #030 (baseline_file_flag), #073 (tech_debt_alignment_cleanup), #045 (docs_sync_roadmap_mvp_spec)

## Цель

Собрать уже существующие куски `--diff` / baseline / report / CI в **один
канонический пользовательский workflow**, которому можно доверять как первому
продуктовому wedge `archcheck`.

Задача не про новые графовые метрики, а про **productization**:

- один понятный сценарий запуска в CI;
- один понятный отчёт;
- одно понятное поведение exit codes;
- явная граница между advisory и gated режимом.

## Контекст

Фундамент уже шипится:

- `#018` дал `--diff`;
- `#023` и `#024` сделали diff-путь быстрым;
- `#025` показал путь в PR/CI;
- `#030` дал baseline;
- rules/report core уже живёт в продукте.

Но остаток всё ещё размазан по нескольким задачам и файлам. Сейчас есть
техническая реализация diff-анализа, но нет одной чёткой product-story уровня:

> “Вставь одну команду в CI и узнай, внёс ли этот PR новую плохую зависимость.”

Пока этого нет, MVP выглядит как набор фич, а не как собранный продуктовый путь.

## Что должно получиться

Для пользователя должен существовать **один канонический workflow**:

- запустил diff против base;
- увидел, какие новые зависимости/циклы появились;
- понял, advisory это или gated regression;
- получил стабильный text/json output;
- смог скопировать готовый CI пример без чтения half-implemented surfaces.

## Что сделать

### 1. Зафиксировать канонический diff surface

- [x] Выбрать и задокументировать одну основную форму запуска PR/diff анализа:
      `archcheck --diff origin/main..HEAD .` (README Quick start, --help, example yml).
- [x] Явно описать, как эта форма соотносится с baseline-flow и локальным запуском
      (README: `--drift-baseline` = альтернатива с pinned snapshot-файлом).
- [x] Убрать из docs/help двусмысленность между “несколько похожих режимов” и
      “один основной продуктовый путь”.

### 2. Зафиксировать product contract отчёта

- [x] Определить обязательный состав diff-отчёта для MVP v1:
      added edges / new cycles / SCC growth / краткий summary (+ gate-вердикт).
- [x] Сделать `text` и `json` стабильными и детерминированными
      (`--diff --format=json`, схема version 1, новый `diff_json_report`).
- [x] Убрать из default output всё, чему пока нельзя доверять как trusted gate:
      все негейтящие секции помечены `(advisory)`, гейтящие — `(gating)`.

### 3. Явно развести advisory и gate

- [x] Зафиксировать advisory-first default.
- [x] Gated в MVP v1 (решение пользователя 2026-06-12): новые/выросшие циклы
      (≈SF.9) + новые god-headers (≈Lakos.GodHeader). Added edges, cross-area,
      chain/NCCD рост, SATD/test-coevol/LCX — advisory. Strict-режим — YAGNI.
- [x] Привязано к exit codes без слома контракта: `RegressionReport::gates()`,
      exit 1 только на gated; текстовый отчёт завершается `gate: ok|fail`.

### 4. Собрать E2E покрытие workflow

- [x] Сценарии уровня продукта: clean diff, new edge (advisory, exit 0),
      new cycle (gate, exit 1), docs-only fast-path; те же для json.
- [x] Проверяется конечный пользовательский output реального бинаря
      (`tests/integration/diff/diff_workflow_e2e_test.cpp`, ARCHCHECK_BINARY_PATH).

### 5. Дать минимальный CI onboarding

- [x] Один copy-paste пример: `.github/workflows/example_archcheck_pr.yml`
      (обновлены exit-коды и advisory-семантика), залинкован из README.
- [x] Строгий gate режим — решение: не делать (YAGNI), один контракт.
- [x] README / MVP.md / CHANGELOG / --help описывают один и тот же путь.

## Критерий приёмки

- [x] У `archcheck` есть один канонический PR/diff workflow, описанный одинаково в code/docs.
- [x] Default behaviour — advisory-first, без скрытых gating surprises.
- [x] `text/json` output стабилен и проверяется e2e-фикстурами.
- [x] Есть copy-paste CI пример без опоры на research/docs archaeology.
- [x] Пользовательский ответ на вопрос “что делает этот PR с dependency graph?” получается одной командой.

## Журнал

- 2026-06-12: аудит поверхности. Главный гэп: `hasRegression()` гейтил ЛЮБОЕ added
  edge / рост NCCD → каждый живой PR красный (противоречие advisory-first). Также:
  нет JSON для `--diff`, e2e проверяли DTO, README подавал `--drift-baseline` и
  `--diff` как параллельные режимы. Попутно починен README: таблица Default
  thresholds была вклеена внутрь bash-кодоблока Quick start.
- 2026-06-12: реализация (всё одним заходом, гейты зелёные):
  - `gates()` в `RegressionReport` (циклы + god-headers); exit 1 только на gates();
    секции текстового отчёта помечены `(advisory)`/`(gating)`; вердикт `gate: ok|fail`.
  - `--diff --format=json`: новый `diff/diff_json_report.{h,cpp}` (схема v1:
    refs / gate / gating{} / advisory{} c flatten SATD+coevol+LCX violations);
    fast-path тоже эмитит валидный JSON. `consumeDiffFlags` в main.cpp понимает
    `--diff-mode=` и `--format=` в любом порядке.
  - diff_command рефакторинг: collect/print разведены (DiffAdvisories),
    `loadDiffThresholds`/`emitJsonDiff` извлечены (lizard-лимиты).
  - E2E: `tests/support/git_test_repo.h` (общий хидер; миграция двух старых
    копий TempDir — в #108) + `diff_workflow_e2e_test.cpp` — 6 кейсов через
    реальный бинарь (`$<TARGET_FILE:archcheck>`), text+json, exit-коды.
  - Docs: README (канонический PR-блок, фикс кодоблока, линк на example yml),
    MVP.md §4, CHANGELOG (Added: JSON; Changed: advisory-first), example yml,
    --help.
  - Гейты: 1688 assertions / 491 кейс зелёные; dogfood src/include/tests = 0;
    clang-format 18.1.3 чисто; lizard — только pre-existing долг (#108).
  - NB: сборка велась в `build/dev075` (отдельный build dir) — `build/debug`
    занят фоновым прогоном #109; перед закрытием #109 пересобрать `build/debug`
    безопасно (скрипт LCX парсит только секцию `local complexity drift
    (advisory):`, формат не менялся, exit-код не используется).

- 2026-06-12 (закрытие): coverage gate сначала упал (85.5% < 90%) — e2e впервые
  заставили coverage-прогон исполнять CLI-бинарь, и весь ранее не измерявшийся
  CLI-слой вошёл в базу с нулями. По решению пользователя добавлен CLI smoke e2e
  (`tests/integration/cli/cli_smoke_e2e_test.cpp`, 10 кейсов: help/version/check/
  json/baselines/previews/dispatch-ошибки) → 91.4% lines / 96.5% functions / 57.6%
  branches, PASS. Попутно: cppcheck 1.86 не парсил init-statement в `else if`
  (`local_complexity_drift.cpp`, код #109) — переписано без init-statement
  (GCC8-COMPAT-маркер); хелпер запуска бинаря вынесен в
  `tests/support/run_archcheck.h` (дедуп между двумя e2e-файлами).

## Как работает

`--diff a..b` строит include-графы обеих сторон (in-memory через git blobs либо
disk worktree), сравнивает их в `RegressionReport` и печатает text- или
JSON-отчёт. Гейт — `RegressionReport::gates()`: exit 1 только при новых/выросших
циклах или новых god-headers (то, что в check-режиме было бы нарушением SF.9 /
Lakos.GodHeader); все остальные дельты и scan-сигналы (SATD, test co-evolution,
LCX) печатаются как advisory и на exit-код не влияют. JSON-контракт (schema v1,
`diff_json_report.cpp`) отделяет блок `gating` от `advisory` и закрывается
полем `gate: ok|fail`. E2E-тесты гоняют реальный бинарь (`$<TARGET_FILE>`)
на синтезированных git-репо и проверяют конечный stdout + exit-код.

## Ключевые решения

- **Gated = циклы + god-headers, без strict-флага** (пользователь, 2026-06-12):
  зеркально `--drift-baseline` (DRIFT.1/2), rule-equivalent классы; added edges —
  нормальная жизнь PR, гейтить их = красный на каждом `#include` (anti-pattern
  «5000 violations»). Strict-режим — YAGNI до запроса реального пользователя.
- **`hasRegression()` сохранён** (= «есть что показать»), `gates()` добавлен
  рядом — 15 существующих тестов не потребовали переосмысления семантики.
- **JSON для diff — отдельный модуль**, не расширение `report::writeJsonReport`:
  у check и diff разные документы (violations-список vs gating/advisory-структура).
- **E2E через реальный бинарь**, не через линковку cli в тесты: проверяется
  именно продуктовый путь (dispatch, флаги, exit-коды), заодно coverage увидел
  CLI-слой.

## Изменённые файлы

- `include/archcheck/diff/regression_report.h`, `src/diff/regression_report.cpp` —
  `gates()`, маркеры `(advisory)`/`(gating)`, вердикт `gate: ok|fail`.
- `include/archcheck/diff/diff_json_report.h`, `src/diff/diff_json_report.cpp` —
  JSON-документ schema v1 (новые).
- `src/cli/diff_command.{h,cpp}` — exit по `gates()`, collect/print развод,
  `OutputFormat` параметр, JSON fast-path.
- `src/main.cpp` — `consumeDiffFlags` (`--diff-mode=` + `--format=`), help.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt` — регистрация; e2e получают
  `ARCHCHECK_BINARY_PATH`.
- `tests/unit/diff/regression_report_test.cpp` — gates()-ассерты, новые маркеры.
- `tests/support/git_test_repo.h`, `tests/support/run_archcheck.h` — общий
  e2e-скаффолдинг (новые).
- `tests/integration/diff/diff_workflow_e2e_test.cpp`,
  `tests/integration/cli/cli_smoke_e2e_test.cpp` — продуктовые E2E (новые).
- `README.md`, `docs/MVP.md`, `CHANGELOG.md`,
  `.github/workflows/example_archcheck_pr.yml` — один канонический workflow.

## Итог

**Статус:** completed
**Дата завершения:** 2026-06-12

## Не делать в этой задаче

- не тащить duplication в release gate;
- не расширять продукт в сторону AI-attribution;
- не делать runtime config policy (`layers` / `forbidden` / `independence`) — это уже `v0.2`;
- не смешивать с semantic backend / libclang expansion.

## Следующие шаги

1. Сначала закрыть `#073` P0-срез по контрактам.
2. После этого собрать и зафиксировать канонический diff contract.
3. Потом уже добирать richer metrics и optional integrations.
