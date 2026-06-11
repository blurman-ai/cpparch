# [CHEAP-DRIFT][DIFF] Test Co-Evolution Signal

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-11
**Статус:** wip
**Модуль:** GIT / DIFF / REPORT
**Приоритет:** major
**Сложность:** small
**Блокирует:** —
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #024 (in_memory_fs_for_diff), #075 (trusted_diff_workflow), #096 (satd_delta)

## Цель

Подсвечивать PR, где production-код заметно меняется, а тесты не двигаются вообще или почти не двигаются.

Это review-prioritization signal, не архитектурное правило и не суррогат test coverage.

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 5.
- Задача особенно полезна в CI review-потоке, где уже есть `git diff --numstat`-уровень сигнала и нужен дешёвый вопрос "а тесты вообще трогали?".
- В репозитории уже есть `scan/file_classification.h` с логикой test/vendor/generated classification. Эту задачу нужно по возможности опереть на существующую таксономию, а не заводить второй несовместимый набор path rules.
- Сигнал принципиально advisory-only по умолчанию. Он не доказывает отсутствие тестов и не должен ломать CI без явного opt-in.

## План выполнения

### Detection contract

- Собрать production churn и test churn по `git diff --numstat`.
- Считать отдельными bucket-ами:
  - production paths;
  - test paths;
  - everything else.
- Finding возникает, когда:
  - production churn выше минимального порога;
  - test churn ниже порога или ниже ratio относительно production churn.
- Конкретные дефолты v1 (калибровать на dogfood + 2-3 OSS PR):
  - `prod_churn = added+deleted` по production-бакету; finding при
    `prod_churn >= 80` строк И `test_churn == 0`;
  - мягкий вариант: `prod_churn >= 200` И `test_churn / prod_churn < 0.05`;
  - rename-only пары (numstat с `=>`) исключить из churn обоих бакетов;
  - header-only изменения (только `.h` без `.cpp`) — не исключать (это тоже код),
    но докудать в message состав бакетов, чтобы reviewer видел причину;
  - один finding на diff: `TEST.1.prod_changed_tests_silent`,
    message = `prod +A/-D across N files, tests +0/-0` (числа реальные).

### Runtime shape

- Первый проход опирать на numstat и текущую file classification.
- Docs-only PR не должны срабатывать.
- Rename-only и generated/vendor cases должны либо игнорироваться, либо быть явно задокументированы как ограничения.
- Для `Violation` достаточно `file = "<diff>"` и `line = 0`, если точной строковой привязки нет; invent fake line numbers не нужно.

### Scope discipline

- Не пытаться вычислять "семантическую значимость" изменения тестов.
- Не строить coverage integration.
- Не превращать правило в hard gate по умолчанию.

### Конкретный план в текущем коде

1. Не парсить `git diff --numstat` второй раз отдельно от #096:
   использовать тот же `git/diff_query.cpp` и его `collectNumstat(...)`.
2. Path classification строить из уже существующих функций, а не из нового списка globs:
   `hasProjectExtension()` из [src/scan/project_files.cpp](/home/localadm/projects/cpparch/src/scan/project_files.cpp),
   `pathHasTestDir()`, `isTestBasename()`, `pathHasVendoredDir()`, `isVendoredFile()` из [include/archcheck/scan/file_classification.h](/home/localadm/projects/cpparch/include/archcheck/scan/file_classification.h).
3. Production bucket определять как "project extension && not test && not vendor".
   Этого достаточно для v1 и оно совпадает с уже shipped exclusions.
4. Detector держать плоским и маленьким (`src/scan/test_co_evolution.cpp` либо соседний helper), вход:
   `std::vector<NumstatEntry>`, выход: либо пусто, либо один `Violation` с `file = "<diff>"`, `line = 0`.
5. Интегрировать рядом с `run_diff()` в [src/main.cpp](/home/localadm/projects/cpparch/src/main.cpp), а не через `IRule` и не через `RegressionReport`.
6. JSON для `--diff` подчиняется тому же реальному ограничению, что и #096:
   пока `dispatch_format()` не станет ортогональным к режиму запуска, у diff path нет machine-readable output.
7. Тесты:
   parsing/aggr — новый `tests/unit/git/diff_query_test.cpp`;
   rule logic — новый `tests/unit/scan/test_co_evolution_test.cpp`;
   repo-level smoke — 1-2 сценария в [tests/integration/diff/git_diff_test.cpp](/home/localadm/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/test_co_evolution/pass/with_tests.numstat`
- `fixtures/test_co_evolution/fail/prod_without_tests.numstat`
- `fixtures/test_co_evolution/pass/docs_only.numstat`
- `fixtures/test_co_evolution/pass/generated_or_vendor_ignored.numstat`

Обязательные проверки:

- churn production vs tests считается корректно;
- docs-only diff не даёт finding;
- PR с достаточным test churn не даёт finding;
- сообщение содержит production churn, test churn и ratio/threshold;
- output совместим с текущим text/json contract.

## Критерии готовности

- Advisory-only по умолчанию.
- Реиспользуется текущая классификация test/vendor/generated, где это возможно.
- Нет нового config schema block в рамках этой задачи.
- Сигнал не маскирует structural diff findings и не ломает их exit semantics.

## Не делать

- Coverage analysis.
- Per-test impact analysis.
- Попытку доказать, что "тестов точно нет".
- Gate by default.

## Сделано

**Детектор test_co_evolution:**
- `include/archcheck/scan/test_co_evolution.h` — заголовок детектора (публичный интерфейс)
- `src/scan/test_co_evolution.cpp` — реализация:
  - `aggregateChurn()` — разделение path по bucket-ам (production/test/other), подсчёт churn
  - `shouldReportCoEvolution()` — проверка двух порогов (strict: 80 строк + 0 тестов, soft: 200 строк + <5% тестов)
  - `detectTestCoEvolution()` — публичная функция с выходом ViolationList (0 или 1 элемент)
  - Используются существующие функции: `hasProjectExtension()`, `pathHasTestDir()`, `isTestBasename()`, `pathHasVendoredDir()`, `isVendoredFile()`

**Интеграция в main.cpp:**
- Добавлен include `#include "archcheck/scan/test_co_evolution.h"`
- Вызов после SATD-блока в `runDiffFullPath()`: `archcheck::git::collectNumstat()` → `detectTestCoEvolution()` → вывод (advisory)
- Формат вывода аналогичен SATD: "test co-evolution (advisory):" + список нарушений

**Фикстуры в `fixtures/test_co_evolution/`:**
- `pass/with_tests.numstat` — production 80 строк + tests 25 строк (no finding)
- `fail/prod_without_tests.numstat` — production 100 строк + tests 0 (finding по strict threshold)
- `pass/docs_only.numstat` — только .md файлы (no finding, no project extension)
- `pass/generated_or_vendor_ignored.numstat` — vendor + rename-only (no finding)

**Unit-тесты в `tests/unit/scan/test_co_evolution_test.cpp`:**
- Парсер для .numstat формата (сырые tab-separated lines)
- 9 тестов, покрывающих:
  - strict threshold: prod 100 + tests 0 → finding
  - insufficient tests: prod 100 + tests 20 → no finding
  - docs-only: no finding
  - rename-only: no finding, не считаются в churn
  - soft threshold: prod 250 + tests 5 (2%) → finding
  - vendor exclusion: vendor files не считаются prod
  - смешанный случай: vendor + prod + tests
  - boundary case: ровно на 5% ratio → no finding (< not <=)

**Интеграционный тест в `tests/integration/diff/git_diff_test.cpp`:**
- `test_co_evolution: production change without test update triggers detection`
  - git repo с src/main.cpp + tests/test_main.cpp
  - baseline commit, затем изменение 85 строк в prod без изменения тестов
  - проверка наличия TEST.1.prod_changed_tests_silent в выводе

**CMakeLists.txt:**
- `src/CMakeLists.txt`: добавлен `scan/test_co_evolution.cpp` в target `archcheck_core`
- `tests/CMakeLists.txt`: добавлен `unit/scan/test_co_evolution_test.cpp` в target `archcheck_tests`

**Качество кода:**
- clang-format: отформатировано (120 columns, Allman braces, 2 spaces)
- cppcheck: OK (без warning/error)
- lizard: 12 CCN, 23 NLOC (в пределах лимитов 15/30)
- Все 390 unit+integration тестов пройдены
- Самопроверка archcheck: src/, include/, tests/ дают 0 нарушений (SF.7/8/9/21, cycles, god-headers, chain-length)

## В работе

- (пусто)

## Следующие шаги

1. Выделить numstat parsing как переиспользуемый helper.
2. Согласовать production/test path classification с уже существующей логикой в репо.
3. Добавить fixtures на churn ratio.
4. Проверить поведение на самом `archcheck`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Advisory-only | Это review signal, не доказательство бага |
| Reuse текущей file classification | Нельзя плодить расходящиеся правила "что считать тестом" |
| `file="<diff>"`, `line=0` допустимы | У numstat нет точной строковой привязки |
| Ratio-heuristic, а не binary "тесты есть/нет" | Иначе шум на маленьких PR будет слишком высоким |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/git/` или diff orchestration | Numstat parsing |
| `src/main.cpp` / cheap-drift pass | Подключение test-co-evolution signal |
| `tests/unit/` | Churn aggregation и classification |
| `tests/integration/` | Diff-based сценарии |
| `fixtures/test_co_evolution/` | `pass/` и `fail/` numstat fixtures |
