# [GRAPH] Graph container — NodeId + dependency_graph

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** GRAPH
**Приоритет:** blocker
**Сложность:** S (< 1 дня)
**Блокирует:** #014 (graph_algorithms)
**Заблокирован:** —
**Related:** #008 (dependency_graph_foundation)

## Цель

Реализовать минимальный канонический контейнер file-level графа: `NodeId`,
нормализация путей, add node/edge, adjacency, reverse adjacency, edge
existence.

## Сделано

- **2026-05-26** — создан `include/archcheck/graph/node_id.h` (`struct NodeId`, `operator==/!=`, `std::hash` специализация).
- **2026-05-26** — создан `include/archcheck/graph/dependency_graph.h` (публичный API: `add_node`, `add_edge`, `has_edge`, `successors`, `predecessors`, `node_count`, `path_of`).
- **2026-05-26** — реализован `src/graph/dependency_graph.cpp` (нормализация POSIX + `./`-prefix, идемпотентные `add_node` / `add_edge`, синхронные forward + reverse adjacency).
- **2026-05-26** — `archcheck_core` пополнен `graph/dependency_graph.cpp` в `src/CMakeLists.txt`.
- **2026-05-26** — добавлен `tests/unit/graph/dependency_graph_test.cpp` (12 кейсов), подключён в `tests/CMakeLists.txt`.
- **2026-05-26** — Debug-сборка зелёная, `ctest` 51/51 (39 прежних + 12 новых graph-кейсов).
- **2026-05-26** — `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — чисто.

## Как работает

Контейнер хранит файлы как `NodeId` — обёртку вокруг `std::uint32_t`, выдаваемую монотонно: `id.value == index` в `paths_`. Это даёт `O(1)` `path_of` и компактный хеш.

`add_node` нормализует путь до укладки в `path_to_id_`:
- `\` → `/` (POSIX-сепаратор);
- ведущие `./` срезаются, поддерживается несколько уровней (`./././foo` → `foo`).

Если нормализованный путь уже есть в `path_to_id_` — возвращается прежний `NodeId` (идемпотентность).

`add_edge(from, to)` синхронно пишет в обе таблицы — `forward_[from]` и `reverse_[to]` — но только если ребра ещё нет (линейная проверка через `std::find`; для file-level графа fan-out маленький). Эта симметрия — инвариант: каждый edge живёт в обеих половинах, что делает `predecessors` дешёвой операцией без перестройки.

`successors` / `predecessors` возвращают `const std::vector<NodeId>&`, при отсутствии узла — пустой статический вектор (чтобы вызывающий мог итерироваться без проверок).

## Чем управляется

- Никаких флагов / переменных среды на этом этапе.
- Подключение через `archcheck::core` (CMake target).
- Поведение нормализации фиксировано в `normalize_path` внутри `dependency_graph.cpp` (POSIX-сепаратор + срез ведущих `./`).

## С чем связана

- Подсистема `graph/` — первый файл в `src/graph/` и в `include/archcheck/graph/`.
- Зависимости: только STL (`<unordered_map>`, `<vector>`, `<string>`, `<algorithm>`).
- `archcheck_core` (STATIC) теперь содержит `scan/include_scanner.cpp` и `graph/dependency_graph.cpp`.
- Передаёт эстафету #014 (алгоритмы: SCC, reachability, levelization) — этот контейнер их хранит, но не реализует.

## Диагностика

- Если линкер не находит `DependencyGraph::*` — проверить, что `src/graph/dependency_graph.cpp` присутствует в списке источников `archcheck_core` в `src/CMakeLists.txt`.
- Если тест не находит хедер — проверить, что `${CMAKE_SOURCE_DIR}/include` остаётся `PUBLIC` у `archcheck_core` (тесты линкуются с `archcheck::core` и наследуют include path).
- Если `add_node` возвращает разные `NodeId` для логически одинаковых путей — путь не приведён к каноничной форме до вызова; нормализация в `dependency_graph.cpp` намеренно консервативная (только POSIX-сепаратор и ведущие `./`), а не полноценный `lexically_normal()`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Reverse adjacency хранится сразу | Lakos Ca / reverse reachable дешевле получать сразу, чем перестраивать |
| `NodeId` — `uint32_t` wrapper | Дешёвый хеш, меньше памяти, чем хранить strings везде |
| Возвращаем `const std::vector<NodeId>&`, не `std::span` | API простой, без C++20-зависимостей у вызывающего; пустой статический вектор для отсутствующих узлов |
| Идемпотентность `add_edge` через линейный `find` | File-level fan-out мал; `unordered_set` на каждом узле — лишний оверхед памяти |
| `normalize_path` — минимальная (POSIX + `./`), не `std::filesystem::lexically_normal` | YAGNI: задача — устранить тривиальные дубли, не строить нормализатор; `..` и абсолютные пути обработает resolver (#012) |
| Один файл — один класс (без `IGraph`) | docs/code_quality.md — нет абстракций без запроса; container и алгоритмы (#014) раздельные классы по плану |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/graph/node_id.h` | new |
| `include/archcheck/graph/dependency_graph.h` | new |
| `src/graph/dependency_graph.cpp` | new |
| `src/CMakeLists.txt` | добавлен `graph/dependency_graph.cpp` |
| `tests/unit/graph/dependency_graph_test.cpp` | new |
| `tests/CMakeLists.txt` | подключён новый test source |
