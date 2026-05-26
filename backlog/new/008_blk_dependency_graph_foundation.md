# [GRAPH] Фундамент графа зависимостей и diff-примитивы

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** GRAPH
**Приоритет:** blocker
**Сложность:** M (2-4 дня)
**Блокирует:** #009 (ai_drift_regression_rules)
**Заблокирован:** #004 (project_skeleton)
**Related:** #006 (spec_refactor)

## Цель

Зафиксировать и реализовать канонический file-level dependency graph и
отдельный `graph-baseline` contract, без которых нельзя надёжно реализовывать
первый прототип `DRIFT.*`.

## Контекст

В спеке появился новый класс `drift-regression rules`, который смотрит не на
“плохой код вообще”, а на структурное ухудшение графа относительно baseline.
Первый прототип намеренно узкий: только file-level graph, только `DRIFT.1` и
`DRIFT.2`, без git history и без repo inference.

Для этого нужен не просто разовый include-graph, а устойчивая внутренняя
модель графа, пригодная для:

- построения из результатов scan-подсистемы;
- вычисления SCC и reachability;
- сравнения baseline-графа и графа после изменения;
- последующего хранения и загрузки минимального `graph-baseline`.

Эта задача блокирует реализацию первого прототипа `DRIFT.*` и часть будущих
Lakos/graph-based checks.

## План выполнения

- [ ] Зафиксировать канонический уровень графа для v0.1: `file-level`
- [ ] Зафиксировать, что module-level представления строятся как проекция поверх file graph, а не как отдельная первичная модель
- [ ] Определить, что считается dependency edge в fast-backend для v0.1
- [ ] Описать формат внутренних идентификаторов узлов (`NodeId`) и нормализацию путей
- [ ] Спроектировать `component_graph` / `dependency_graph` API без лишней абстракции
- [ ] Реализовать операции: add node, add edge, adjacency, reverse adjacency, edge existence
- [ ] Реализовать graph algorithms, нужные как примитивы: SCC, reachable set, reverse reachable set, path existence
- [ ] Добавить diff-примитивы для первого прототипа: new edges, removed edges, grown SCC
- [ ] Зафиксировать `graph-baseline` contract: `format version + normalized nodes + normalized edges`
- [ ] Подготовить fixtures / integration samples для маленьких графов и их diff-сценариев

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Сначала зафиксировать file-level семантику графа и формат baseline
2. Затем описать минимальный API графа и набор алгоритмов
3. После этого перейти к реализации `DRIFT.1` / `DRIFT.2`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Сначала graph primitives, потом rules | Иначе `DRIFT.*` и Lakos checks будут опираться на неустойчивую модель |
| Канонический граф — file-level | Это упрощает семантику и устраняет дублирование первичных моделей |
| Нужен именно graph diff, а не только snapshot | AI-drift rules по смыслу сравнивают состояние “до/после” |
| baseline хранит snapshot, а не derived metrics | Это делает формат устойчивее и не привязывает его к текущему набору правил |
| Минимум зависимостей, без внешней graph-библиотеки | Совпадает с архитектурными ограничениями проекта |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| src/graph/* | будущая реализация графа и алгоритмов |
| tests/unit/graph/* | будущие unit tests |
| tests/integration/graph/* | будущие integration fixtures |
| docs/architecture-spec.md | уточнение `graph-baseline` и file-level contracts |

## Fixtures

- [ ] Минимальный DAG без циклов
- [ ] Граф с одним SCC
- [ ] Сценарий “появилось новое ребро”
- [ ] Сценарий “shortcut edge”
- [ ] Сценарий “cycle growth”
