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

- [ ] Создать `fixtures/drift_shortcut_edge/pass/` — файлы идентичны baseline, нарушений нет
- [ ] Создать `fixtures/drift_shortcut_edge/fail_new_coupling/` — `a.h` теперь включает `b.h`, которого не было в baseline
- [ ] Создать `fixtures/drift_cycle_growth/pass/` — цикл не изменился относительно baseline
- [ ] Создать `fixtures/drift_cycle_growth/fail_new_cycle/` — новый цикл `a.h ↔ b.h`
- [ ] Написать интеграционный тест в `tests/integration/rules/drift_fixtures_test.cpp`
- [ ] Зарегистрировать в `tests/CMakeLists.txt`

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Создать `.h` файлы для каждой фикстуры
2. Сгенерировать `baseline.graph.yml` через `archcheck --save-graph-baseline` на "baseline"-варианте фикстуры
3. Написать тест, аналогичный `scanner_fixtures_test.cpp` и `end_to_end_test.cpp`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| baseline.graph.yml хранится прямо в папке фикстуры | Фикстура самодостаточна, тест не генерирует baseline на лету |
| pass/ = файлы совпадают с baseline | Нет изменений → нет нарушений DRIFT |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `fixtures/drift_shortcut_edge/` | новый |
| `fixtures/drift_cycle_growth/` | новый |
| `tests/integration/rules/drift_fixtures_test.cpp` | новый |
| `tests/CMakeLists.txt` | добавить новый тест |

## Fixtures

- [ ] `fixtures/drift_shortcut_edge/pass/`
- [ ] `fixtures/drift_shortcut_edge/fail_new_coupling/`
- [ ] `fixtures/drift_cycle_growth/pass/`
- [ ] `fixtures/drift_cycle_growth/fail_new_cycle/`
