# [GRAPH] Graph algorithms — SCC + reachability

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** GRAPH
**Приоритет:** blocker
**Сложность:** M (1-2 дня)
**Блокирует:** #015 (diff_primitives)
**Заблокирован:** #013 (graph_container)
**Related:** #008 (dependency_graph_foundation)

## Цель

Реализовать алгоритмы-примитивы: SCC (Tarjan), forward reachable set, reverse
reachable set, path existence. Каждый — отдельная функция, без общего
«engine».

## Сделано

- **2026-05-26** — создан `include/archcheck/graph/algorithms.h` (объявления `compute_scc`, `reachable_from`, `reverse_reachable_from`, `has_path` в `archcheck::graph`).
- **2026-05-26** — реализован `src/graph/algorithms.cpp`: iterative Tarjan на явном стеке кадров, BFS-обходы по successors / predecessors, `has_path` с ранним выходом.
- **2026-05-26** — `archcheck_core` пополнен `graph/algorithms.cpp` в `src/CMakeLists.txt`.
- **2026-05-26** — добавлен `tests/unit/graph/algorithms_test.cpp` (14 кейсов: пустой граф, DAG, единый цикл, два компонента, детерминизм, симметрия forward / reverse, self-path, направление рёбер), подключён в `tests/CMakeLists.txt`.
- **2026-05-26** — Debug-сборка зелёная, `ctest` 76/76.
- **2026-05-26** — `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — чисто.

## Как работает

Четыре free-function в `archcheck::graph` — никакого общего «engine», каждый алгоритм самостоятелен.

**`reachable_from` / `reverse_reachable_from`** — общий внутренний `bfs(g, start, forward)` обходит соседей через `successors` либо `predecessors` в зависимости от флага. Старт включается в результат. Хеш-сет `visited` одновременно служит и фильтром повторов, и возвращаемым множеством.

**`has_path`** — отдельный BFS, который не строит полное множество достижимости: проверяет цель сразу при раскрытии соседа и выходит. Случай `from == to` обрабатывается заранее (тривиальный self-path всегда true, чтобы не зависеть от наличия петли в графе).

**`compute_scc`** — iterative Tarjan на явном стеке кадров. Состояние держится в структуре `TarjanState`:
- `index_of`, `lowlink_of` — массивы по `node_count()`, индексируемые `NodeId.value` (мы используем тот факт, что id — плотный диапазон 0..n-1, см. #013);
- `on_stack` — `vector<bool>` по `node_count()` для O(1) проверки «нода ещё в текущей компоненте»;
- `component_stack` — стек узлов S из классического Tarjan;
- `call_stack` — стек **кадров** `{node, succ_index}`, заменяющий C++-рекурсию;
- `next_index` — монотонный счётчик DFS-порядка;
- `result` — выходные компоненты.

Машина состояний (`run_tarjan_from`):
1. Кладём корень в `call_stack`, `open_node` присваивает ему индекс/lowlink, кладёт на `component_stack`, помечает `on_stack`.
2. Главный цикл: пока `call_stack` непуст, смотрим верхний кадр.
3. `step_into_successor` идёт по `successors(frame.node)` начиная с `frame.succ_index`:
   - если сосед ещё не посещён — кладём новый кадр для него и возвращаем `true` (главный цикл вызовет `open_node` для нового кадра и продолжит работу с ним);
   - если сосед `on_stack` — `lowlink[v] = min(lowlink[v], index[w])` (back-edge внутри текущей SCC);
   - иначе — сосед в уже эмитированной SCC, игнорируется;
   - `succ_index` инкрементируется при каждом проходе, чтобы при возврате к кадру не пересматривать обработанные рёбра.
4. Когда соседи исчерпаны, `close_node` проверяет `lowlink[v] == index[v]` — корень SCC — и `emit_component` вынимает узлы из `component_stack` до v включительно. Затем кадр снимается, и `lowlink` родителя обновляется значением `lowlink[v]` (этот шаг заменяет «после возврата из рекурсии» в учебной версии).
5. Внешний цикл `compute_scc` гоняет `run_tarjan_from` для каждого корня с `index_of[i] == kUnvisited`.

После сбора SCC `sort_components` сортирует узлы внутри каждой компоненты и сами компоненты по `NodeId.value` минимального узла — это даёт детерминированный вывод вне зависимости от порядка обхода (Tarjan сам по себе порядок-зависим).

## Чем управляется

- Никаких флагов / переменных среды.
- Подключение через `archcheck::core` (CMake target).
- Сортировка SCC жёстко зашита по возрастанию `NodeId.value`; отдельной опции «исходный Tarjan-порядок» нет — детерминизм — это инвариант для CI.

## С чем связана

- Зависит от `DependencyGraph` (#013): использует `node_count()`, `successors()`, `predecessors()`. NodeId здесь трактуется как плотный индекс 0..n-1 — это контракт контейнера, зафиксированный в #013.
- Зависимости вне репо: только STL (`<algorithm>`, `<queue>`, `<unordered_set>`, `<vector>`, `<limits>`, `<cstdint>`).
- Передаёт эстафету #015 (diff primitives): добавленные / удалённые рёбра и компоненты будут вычисляться поверх этих SCC и reachability.
- Эти же примитивы лягут в основу правил cycle detection (Lakos) и метрик CCD / ACD / NCCD (#006 / Martin-метрики в v0.2+).

## Диагностика

- Если `compute_scc` возвращает нестабильный порядок на одинаковом входе — это баг сортировки, проверить `sort_components`: компараторы должны сравнивать по `NodeId.value`, а не по адресу.
- Если на больших графах ловится stack overflow — алгоритм деградировал к рекурсии; убедиться, что `run_tarjan_from` остался на while-цикле с явным `call_stack`, без вызовов самого себя.
- Если `reachable_from(g, x)` не содержит `x` — забыта вставка стартового узла в `visited` до основного цикла BFS.
- Если `has_path(g, a, a)` вернул `false` для узла без петли — пропущена ранняя проверка `from == to` в начале функции.
- `std::unordered_set<NodeId>::contains` — только GCC 9+ / C++20-lib. В тестах используем `count(x) == 1`, потому что в окружении встречается GCC 8.3 (см. `stdc++fs`-фолбэк в CMake).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Tarjan iterative на явном стеке кадров | Рекурсия упадёт по stack overflow на 100k+ файлов |
| `vector<uint32_t>` для `index_of` / `lowlink_of`, не `unordered_map` | NodeId — плотный 0..n-1 (контракт #013), массив быстрее и компактнее |
| `kUnvisited = UINT32_MAX` в роли «не посещён» | Не требует параллельного `visited`-массива; UINT32_MAX явно невозможен как реальный DFS-индекс |
| Helpers `open_node` / `close_node` / `step_into_successor` / `emit_component` | Дробление под порог lizard 30 строк / CCN 15; каждая функция отвечает за один шаг машины состояний |
| Общий `bfs(g, start, forward)` для forward / reverse | Логика идентична, ветвление только по выбору `successors` vs `predecessors`; дублирующий код был бы AI-слопом |
| `has_path` — отдельная функция, не `reachable_from(g, from).count(to)` | Ранний выход экономит обход на больших графах с близкой целью |
| Детерминированный порядок SCC через сортировку по min `NodeId` | CI-инструмент обязан давать одинаковый вывод на одинаковом входе; Tarjan сам по себе порядок-зависим |
| Free functions, не методы `DependencyGraph` | План #008: «без лишней абстракции»; контейнер и алгоритмы развиваются независимо |
| `r.count(x) == 1` в тестах вместо `r.contains(x)` | `unordered_set::contains` — C++20, доступен с GCC 9; в окружении встречается GCC 8.3 |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/graph/algorithms.h` | new |
| `src/graph/algorithms.cpp` | new |
| `tests/unit/graph/algorithms_test.cpp` | new |
| `src/CMakeLists.txt` | добавлен `graph/algorithms.cpp` |
| `tests/CMakeLists.txt` | подключён новый test source |
