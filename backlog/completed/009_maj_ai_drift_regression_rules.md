# [RULES/DRIFT] AI-oriented drift-regression rules

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-28
**Статус:** done
**Модуль:** RULES/DRIFT
**Приоритет:** major
**Сложность:** M (2-4 дня на дизайн, реализация отдельными шагами)
**Блокирует:** —
**Заблокирован:** ~~#008 (dependency_graph_foundation)~~ ✅
**Related:** #006 (spec_refactor)

## Цель

Зафиксировать и затем реализовать первый, намеренно узкий прототип
`drift-regression rules`, который доказывает полезность diff-based structural
analysis на `DRIFT.1` и `DRIFT.2`.

## Контекст

В ходе продуктового обсуждения появилась новая сильная идея: между zero-config
hygiene checks и полностью user-declared architecture policy есть
промежуточный класс правил — `drift-regression rules`.

Рабочее название для соседней линии, где агент читает репозиторий и выводит
проверяемые архитектурные гипотезы: **AI-assisted rule synthesis**.

Их задача — отвечать не на вопрос "что у вас вообще плохо", а на вопрос:

> какой локально-удобный, но глобально-размывающий архитектуру шаг был только что сделан в изменении?

Для первого прототипа scope жёстко сужен до `DRIFT.1` и `DRIFT.2`.

## План выполнения

- [x] Зафиксировать `DRIFT.1` и `DRIFT.2` как единственный scope первого прототипа
- [x] Реализовать `DRIFT.1 no_new_shortcut_edge` + unit tests
- [x] Реализовать `DRIFT.2 no_new_cycle_or_cycle_growth` + unit tests
- [x] Зафиксировать suppression-механику: `--save-graph-baseline` → обновить граф-baseline
- [x] Привязать правила к graph-baseline через конструктор (не к violation baseline)
- [x] CLI: `--drift-baseline <file>` + `--save-graph-baseline <file>`
- [ ] Подготовить fixtures (`drift_shortcut_edge`, `drift_cycle_growth`)
- [ ] Вынести `DRIFT.3+` в отдельную follow-up задачу

## Сделано

- **DRIFT.1 `DriftNoShortcutEdge`**: новое ребро между двумя файлами, оба из которых существовали в baseline. Новые файлы не флагируются. 6 unit tests, все зелёные.
- **DRIFT.2 `DriftNoCycleGrowth`**: SCC (цикл) вырос или появился новый. Использует `graph::grownSccs()`. 5 unit tests, все зелёные.
- **`makeDriftRuleSet(baseline)`** в `rule_set.h/.cpp` — фабрика для обоих DRIFT-правил.
- **CLI**: `--drift-baseline <file>` запускает дефолтные правила + DRIFT; `--save-graph-baseline <file>` сохраняет граф включений как baseline.
- Полный прогон: 713 assertions, 209 test cases — всё зелёное.

## В работе

- Fixtures для ручного и integration-тестирования (пока нет)

## Следующие шаги

1. Создать `fixtures/drift_shortcut_edge/` и `fixtures/drift_cycle_growth/` (pass + fail)
2. Добавить интеграционный тест, который запускает `--drift-baseline` на фикстуры
3. Завести `backlog/future/` задачу для второй волны `DRIFT.3+`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| DRIFT rules хранят baseline в конструкторе | Чисто вписывается в `IRule` без изменения интерфейса |
| DRIFT.1 флагирует только рёбра, где оба конца в baseline | Новые файлы могут include что угодно — это не shortcut |
| Suppression = обновить граф-baseline через `--save-graph-baseline` | Консистентно с violation baseline паттерном, не нужна отдельная allow-list |
| `--drift-baseline` запускает и дефолтные правила, и DRIFT | Один проход, полный отчёт |
| Severity: warning реализована через стандартный `Violation` (без поля severity) | Поля `ruleId` достаточно — при необходимости добавить severity в `Violation` позже |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/rules/drift_no_shortcut_edge.h` | новый — DRIFT.1 |
| `src/rules/drift_no_shortcut_edge.cpp` | новый — DRIFT.1 impl |
| `include/archcheck/rules/drift_no_cycle_growth.h` | новый — DRIFT.2 |
| `src/rules/drift_no_cycle_growth.cpp` | новый — DRIFT.2 impl |
| `include/archcheck/rules/rule_set.h` | добавлен `makeDriftRuleSet` |
| `src/rules/rule_set.cpp` | реализация `makeDriftRuleSet` |
| `src/CMakeLists.txt` | добавлены два новых .cpp |
| `src/main.cpp` | `--drift-baseline`, `--save-graph-baseline`, `applyDriftFile` |
| `tests/unit/rules/drift_no_shortcut_edge_test.cpp` | новый — 6 тестов |
| `tests/unit/rules/drift_no_cycle_growth_test.cpp` | новый — 5 тестов |
| `tests/CMakeLists.txt` | добавлены два новых теста |

## Fixtures (если правило)

- [ ] `fixtures/drift_shortcut_edge/pass/`
- [ ] `fixtures/drift_shortcut_edge/fail_new_coupling/`
- [ ] `fixtures/drift_cycle_growth/pass/`
- [ ] `fixtures/drift_cycle_growth/fail_new_cycle/`

---

**Дата завершения:** 2026-05-28

## Как работает

Два правила реализуют `IRule` и хранят копию baseline-графа в конструкторе:

- **DRIFT.1 `DriftNoShortcutEdge`**: вызывает `graph::addedEdges(baseline, current)`, фильтрует рёбра где оба конца (`from` и `to`) существовали в baseline. Флагирует каждое такое ребро — это «shortcut»: существующий файл добавил новую зависимость на другой существующий файл. Новые файлы не флагируются.
- **DRIFT.2 `DriftNoCycleGrowth`**: вызывает `graph::grownSccs(baseline, current)`, одно нарушение на каждый выросший или новый SCC ≥ 2.

## Чем управляется

- `archcheck --drift-baseline <file> [path]` — запускает дефолтные правила + DRIFT.1/DRIFT.2.
- `archcheck --save-graph-baseline <file> [path]` — сохраняет граф включений как YAML (suppression: сохранить после ревью → новые рёбра перестанут быть «added»).
- `makeDriftRuleSet(baseline)` в `rule_set.h` — фабрика, создаёт оба правила.

## С чем связана

- `graph::addedEdges()` / `graph::grownSccs()` из `include/archcheck/graph/diff.h`
- `graph::loadBaseline()` / `graph::saveBaseline()` из `include/archcheck/graph/baseline.h`
- `IRule` из `include/archcheck/rules/i_rule.h`
- Фикстуры и интеграционный тест — задача #040

## Диагностика

Если DRIFT.1 не флагирует ожидаемый shortcut: проверить что оба файла действительно есть в baseline (`archcheck --save-graph-baseline` + просмотр YAML). Если DRIFT.2 молчит на новый цикл: проверить что цикл действительно появился только в current (`--diff HEAD~1..HEAD`).
