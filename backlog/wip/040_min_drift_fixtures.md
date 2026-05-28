# [FIXTURES/DRIFT] Integration fixtures for DRIFT.1 and DRIFT.2

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28
**Статус:** wip
**Модуль:** FIXTURES/DRIFT
**Приоритет:** minor
**Сложность:** S (полдня)
**Блокирует:** —
**Заблокирован:** —
**Related:** #009 (ai_drift_regression_rules)

## Цель

Добавить фикстуры и интеграционный тест для `--drift-baseline`, чтобы проверить CLI-пайплайн DRIFT-правил end-to-end (загрузка граф-baseline с диска, сканирование, вывод нарушений).

## Контекст

Unit-тесты DRIFT.1/DRIFT.2 покрывают логику правил. Фикстуры нужны для проверки полного пайплайна: реальные `.h`-файлы + сохранённый `baseline.graph.yml` + запуск `archcheck --drift-baseline`. Особенность DRIFT-фикстур: каждая содержит не только исходники, но и pre-saved `baseline.graph.yml` (срез графа "до").

## План выполнения

- [x] Создать `fixtures/drift_shortcut_edge/pass/` — файлы идентичны baseline, нарушений нет
- [x] Создать `fixtures/drift_shortcut_edge/fail_new_coupling/` — `a.h` добавляет shortcut a→c поверх a→b→c
- [x] Создать `fixtures/drift_cycle_growth/pass/` — цикл не изменился относительно baseline
- [x] Создать `fixtures/drift_cycle_growth/fail_new_cycle/` — новый цикл `a.h ↔ b.h`
- [x] Написать интеграционный тест в `tests/integration/rules/drift_fixtures_test.cpp`
- [x] Зарегистрировать в `tests/CMakeLists.txt`

## Сделано

- Все 4 фикстуры созданы с ручными `baseline.graph.yml` (формат v1)
- `tests/integration/rules/drift_fixtures_test.cpp` — 4 теста, все зелёные
- Добавлен в `tests/CMakeLists.txt`
- Полный suite: 213 test cases / 726 assertions — OK
- lizard чист

## В работе

- (пусто)

## Следующие шаги

1. Коммит и перенос в `completed/`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| baseline.graph.yml хранится прямо в папке фикстуры | Фикстура самодостаточна, тест не генерирует baseline на лету |
| pass/ = файлы совпадают с baseline | Нет изменений → нет нарушений DRIFT |
| fail_new_cycle триггерит и DRIFT.1 и DRIFT.2 | b.h→a.h — новое ребро между двумя узлами baseline → оба правила срабатывают; тест фильтрует по ruleId |
| `build_graph_from_dir` вынесен в отдельный хелпер | lizard --length 30; run_drift_check оставался бы 36 строк |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `fixtures/drift_shortcut_edge/pass/` | новый (a.h, b.h, c.h, baseline.graph.yml) |
| `fixtures/drift_shortcut_edge/fail_new_coupling/` | новый (a.h adds a→c shortcut) |
| `fixtures/drift_cycle_growth/pass/` | новый (a↔b cycle, baseline совпадает) |
| `fixtures/drift_cycle_growth/fail_new_cycle/` | новый (baseline a→b only; current a↔b) |
| `tests/integration/rules/drift_fixtures_test.cpp` | новый — 4 интеграционных теста |
| `tests/CMakeLists.txt` | добавлен новый тест |

## Fixtures

- [x] `fixtures/drift_shortcut_edge/pass/`
- [x] `fixtures/drift_shortcut_edge/fail_new_coupling/`
- [x] `fixtures/drift_cycle_growth/pass/`
- [x] `fixtures/drift_cycle_growth/fail_new_cycle/`
