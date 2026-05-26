# [GRAPH] Graph diff primitives — new/removed edges, grown SCC

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** GRAPH
**Приоритет:** blocker
**Сложность:** S (< 1 дня)
**Блокирует:** #016 (graph_baseline_contract)
**Заблокирован:** #014 (graph_algorithms), #012 (include_resolver)
**Related:** #008 (dependency_graph_foundation), #009 (ai_drift_regression_rules)

## Цель

Реализовать примитивы сравнения двух графов: новые рёбра, удалённые рёбра,
вырос ли SCC относительно baseline.

## Контекст

См. план #008: «diff-примитивы для первого прототипа: new edges, removed
edges, grown SCC». Это та основа, без которой нельзя будет сделать первый
прототип `DRIFT.1` / `DRIFT.2` в #009.

«Grown SCC» = появилось SCC размера ≥ 2 там, где в baseline его не было, или
размер существующего SCC увеличился.

## Сделано

- **2026-05-26** — `include/archcheck/graph/diff.h`: `EdgeRef`, `GrownScc`, три свободные функции в `archcheck::graph`.
- **2026-05-26** — `src/graph/diff.cpp`: реализация `added_edges`, `removed_edges`, `grown_sccs` через сравнение по `path_of(...)`.
- **2026-05-26** — `tests/unit/graph/diff_test.cpp`: 11 кейсов — новое ребро, shortcut, новые узлы, удалённое ребро, исчезнувший конец, no-op, brand-new cycle, grown cycle, unchanged cycle.
- **2026-05-26** — `src/CMakeLists.txt` и `tests/CMakeLists.txt`: подключены новые source-файлы.
- **2026-05-26** — Debug-сборка зелёная, `ctest` 98/98, `lizard --CCN 15 --length 30 --arguments 5` — без замечаний.

## Как работает

Все три функции — pure: на вход два `const DependencyGraph&`, на выходе вектор.
Сравнение **по строке `path_of(...)`**, потому что `NodeId` — счётчик insertion
order и меняется от прогона к прогону.

- `added_edges` строит индекс `path → NodeId` по baseline, проходит по всем рёбрам
  current; ребро добавлено, если хотя бы один из его концов отсутствует в baseline
  по пути, либо если оба пути есть, но ребра нет (`baseline.has_edge(...)` == false).
- `removed_edges` симметрично: индекс по current, обход рёбер baseline. Рёбра, чьи
  концы исчезли из current, **пропускаются** — для drift-репорта они избыточны
  (узел уже удалён, отдельная сигнатура события).
- `grown_sccs` зовёт `compute_scc` на обоих графах, строит для каждого нетривиального
  (`size >= 2`) SCC множество путей, и сопоставляет current-SCC с тем baseline-SCC, с
  которым у него максимальное пересечение по путям (singleton-SCCs baseline игнорируются —
  они «не цикл»). Если matched baseline-SCC меньше current (или совсем не нашёлся),
  current-SCC попадает в результат с соответствующим `baseline_size`.

Возвращаемые `EdgeRef` всегда используют NodeIds **из `current`** — это нужно для
последующего репорта поверх свежего графа.

Итоговые векторы детерминированно отсортированы (`added_edges`/`removed_edges` — по
`(from.value, to.value)`), `grown_sccs` идёт в порядке, в котором их вернул `compute_scc`,
который уже сам стабилен.

## Чем управляется

- Никаких флагов / переменных среды.
- API целиком header-only по форме — три свободные функции, два POD-`struct`'а.
- Доступ через `archcheck::core` (CMake target).

## С чем связана

- **Внутри `graph/`:** опирается на `DependencyGraph::path_of/has_edge/successors/node_count` и `compute_scc` из #014.
- **Дальше по pipeline:** #016 (graph_baseline_contract) — сериализация baseline; #009 (`DRIFT.1`/`DRIFT.2`) — превратит результат diff'а в violation-report.
- **Не трогает:** `scan/`, `config/`, `report/`, libclang — это чисто графовая алгоритмика.

## Диагностика

- Если линкер не находит `added_edges/removed_edges/grown_sccs` — проверить, что
  `graph/diff.cpp` в списке source files `archcheck_core` в `src/CMakeLists.txt`.
- Если `grown_sccs` возвращает «лишние» grown — посмотреть, не подсунули ли в
  baseline ту же версию графа, что и current (например, для unit-теста — сравнить
  `compute_scc(baseline)` и `compute_scc(current)`).
- Если `removed_edges` возвращает пусто там, где ожидаешь ребро — проверить, не
  переименовался ли путь концов (узлы есть, но под другим path → diff считает их
  «новыми + удалёнными», ребро не матчится).
- Тестовый файл `tests/unit/graph/diff_test.cpp` покрывает все ключевые сценарии —
  при изменении семантики diff'а **обновлять его в первую очередь**.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Сопоставление SCC по membership, не по ID | NodeId стабилен только внутри одного запуска, baseline → current не гарантирует совпадения |
| Singleton-SCC в baseline не считаются «matched» | Singleton — это не цикл; иначе brand-new цикл `{a,b,c}` матчился бы singleton-ом `{a}` и не давал `baseline_size=0` |
| `removed_edges` пропускает рёбра, чей конец исчез из current | Drift-репортер обрабатывает «удалённый узел» отдельным сигналом; здесь — только рёбра между ещё живыми узлами |
| `EdgeRef`'ы использует NodeIds `current` графа | Diff потребляется отчётом, который строится поверх current; baseline-NodeIds бесполезны вне baseline'а |
| API DependencyGraph **не расширялся** | Дополнительный аксессор не нужен: NodeIds плотные `[0, n)`, итерация через `for i in [0, n)` + `path_of(NodeId{i})` достаточна, нет смысла протекать private-контейнер |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/graph/diff.h` | new |
| `src/graph/diff.cpp` | new |
| `tests/unit/graph/diff_test.cpp` | new |
| `src/CMakeLists.txt` | добавлен `graph/diff.cpp` в `archcheck_core` |
| `tests/CMakeLists.txt` | добавлен `unit/graph/diff_test.cpp` в `archcheck_tests` |
