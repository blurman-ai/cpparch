# [RULES/DRIFT] AI-oriented drift-regression rules

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** RULES/DRIFT
**Приоритет:** major
**Сложность:** M (2-4 дня на дизайн, реализация отдельными шагами)
**Блокирует:** —
**Заблокирован:** #008 (dependency_graph_foundation)
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

Их задача — отвечать не на вопрос “что у вас вообще плохо”, а на вопрос:

> какой локально-удобный, но глобально-размывающий архитектуру шаг был только что сделан в изменении?

Такие правила особенно полезны в эпоху AI-кодинга: агент часто не создаёт
откровенный мусор, а делает правдоподобный shortcut или локальную структурную
докрутку, которая размывает уже сложившийся граф.

Для первого прототипа scope нужно жёстко сузить:

- `DRIFT.1 no_new_shortcut_edge`
- `DRIFT.2 no_new_cycle_or_cycle_growth`

Остальные `DRIFT.*` правила считаются перспективными, но выносятся во вторую
волну, потому что требуют repo inference, настройки порогов или git history.

## План выполнения

- [ ] Зафиксировать `DRIFT.1` и `DRIFT.2` как единственный scope первого прототипа
- [ ] Для обоих правил зафиксировать входные данные, алгоритм, expected output и false-positive profile
- [ ] Зафиксировать default severity: `warning`
- [ ] Зафиксировать suppression / allow-механику для `DRIFT.1`
- [ ] Привязать оба правила к отдельному `graph-baseline`, а не к обычному violation baseline
- [ ] Подготовить маленькие reference-сценарии для `shortcut edge` и `cycle growth`
- [ ] Вынести `DRIFT.3+` в отдельную follow-up группу

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Держать scope первого прототипа только на `DRIFT.1` и `DRIFT.2`
2. Привязать их к graph primitives из #008
3. После этого заводить follow-up tasks для второй волны `DRIFT.*`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Drift rules — отдельное семейство, а не подвид SF/Lakos | У них другая продуктовая цель: ловить structural regression |
| Первый прототип = только `DRIFT.1` и `DRIFT.2` | Это самые чистые и наименее спорные diff-based правила |
| Первая реализация = `warning`, не `error` | Сначала нужно проверить полезность сигнала на реальных репозиториях |
| Делать ставку на `graph-baseline + diff`, а не на абсолютные smell thresholds | Это лучше бьётся с AI-induced architectural drift |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/architecture-spec.md | scope и формализация первого прототипа `DRIFT.*` |
| src/rules/drift/* | будущая реализация правил |
| tests/integration/drift/* | будущие сценарии |
| tests/unit/rules/drift/* | будущие unit tests |

## Fixtures (если правило)

- [ ] `fixtures/drift_shortcut_edge/pass/`
- [ ] `fixtures/drift_shortcut_edge/fail_*/`
- [ ] `fixtures/drift_cycle_growth/pass/`
- [ ] `fixtures/drift_cycle_growth/fail_*/`
