# [GRAPH] Graph-baseline contract — формат и I/O

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** GRAPH
**Приоритет:** blocker
**Сложность:** M (1-2 дня)
**Блокирует:** #008 (dependency_graph_foundation) — закрывает graph-ветку
**Заблокирован:** #015 (graph_diff_primitives)
**Related:** #008 (dependency_graph_foundation), #009 (ai_drift_regression_rules)

## Цель

Зафиксировать минимальный устойчивый формат `graph-baseline`:
`format_version + normalized nodes + normalized edges`, плюс save/load.

## Контекст

См. §5 mini-design в #008: «`graph-baseline` хранит только nodes + resolved
project edges». Это контракт для CI: baseline лежит в репо, сравнивается с
текущим графом, любые drift-нарушения видны в diff.

Формат — детерминированный (отсортированные nodes/edges), читаемый человеком
(YAML через `ryml`, уже зависит весь core), с явной версией.

## Сделано

- **2026-05-26** — спецификация формата `docs/baseline_format.md` (~1 страница): YAML-схема, правила детерминизма, политика версий, перечень `BaselineLoadError::Kind`.
- **2026-05-26** — публичный API `include/archcheck/graph/baseline.h`: `save_baseline`, `load_baseline`, структурированный `BaselineLoadError`.
- **2026-05-26** — реализация `src/graph/baseline.cpp`: ручной emit YAML (детерминизм без emitter ryml) + парсинг через `ryml::parse_in_arena` с error-callback → exception → `BaselineLoadError`.
- **2026-05-26** — Catch2-тесты `tests/unit/graph/baseline_test.cpp` (10 кейсов): round-trip идемпотентность, детерминированность на разном порядке вставки, отказы по каждому `Kind` ошибки, восстановление топологии.
- **2026-05-26** — `src/CMakeLists.txt` дополнен `graph/baseline.cpp`, `tests/CMakeLists.txt` — новым тестом.
- **2026-05-26** — Debug-сборка зелёная, ctest 98/98, lizard `--CCN 15 --length 30 --arguments 5` чист.

## Как работает

**Формат.** Один YAML-документ:

```yaml
format_version: "1"
nodes:
   - "include/a.h"
   - "src/b.cpp"
edges:
   - [1, 0]
```

`nodes` — отсортированные лексикографически нормализованные пути узлов графа.
`edges` — пары `[from_idx, to_idx]`, индексы — в пост-сортировочной нумерации
`nodes`, сами рёбра отсортированы по `(from, to)`. Это даёт побайтовое
равенство сериализаций двух логически одинаковых графов независимо от
порядка вставки.

**Save** (`save_baseline`):
1. Собрать пути узлов из графа, отсортировать → `sorted_paths`.
2. Построить ремап `old_idx → new_idx`.
3. Пройти все `successors(NodeId{i})` для всех `i`, переписать индексы, отсортировать пары.
4. Эмитить YAML руками: контроль над форматированием стабильнее, чем гонять ryml-emitter ради трёх строк.

**Load** (`load_baseline`):
1. Прочитать поток целиком.
2. `parse_tree`: установить ryml error-callback, кидающий `RymlParseException`, распарсить в arena, восстановить дефолтные коллбэки.
3. Проверить `format_version` (только `"1"`).
4. Распарсить `nodes` и `edges` через отдельные helpers; каждая ошибка → `BaselineLoadError`.
5. Собрать `DependencyGraph` через публичные `add_node` / `add_edge`.

Ошибки представлены `BaselineLoadError{kind, message, line}` и возвращаются в `std::optional` — исключения наружу не пробрасываются.

## Чем управляется

- API-только, флагов / переменных среды нет.
- Подключение через `archcheck::core` (CMake target), `ryml::ryml` тянется
  через PUBLIC-зависимость core'а.

## С чем связана

- Подсистема `graph/` — третий файл после `dependency_graph.cpp` и `algorithms.cpp`.
- Использует только публичный API `DependencyGraph` (`node_count`, `path_of`, `successors`, `add_node`, `add_edge`) — расширять контейнер для этой задачи не понадобилось.
- Готовит почву для #015 (graph-diff primitives) и DRIFT.1 / DRIFT.2 (#009).

## Диагностика

- **Тест с malformed YAML висит без выхода.** Значит дефолтный ryml error-callback победил наш — он вызывает `abort()` / зацикливается, потому что error-handler «не имеет права возвращаться» (см. quickstart §sample_error_handler). Проверить, что `set_callbacks(rcb)` вызывается ДО `parse_in_arena`, а сам callback `[[noreturn]]` и кидает исключение, ловимое внутри `load_baseline`.
- **Round-trip не идемпотентен.** Скорее всего, не отсортированы `edges` после ремапа, либо `sorted_node_paths` потеряли стабильность из-за того, что `path_of` теперь возвращает `string_view` на storage, который мог реаллоцироваться — в текущем коде `paths_` стабилен в пределах жизни графа, но при модификации помнить о string_view'ах.
- **`UnknownVersion` срабатывает в неожиданных местах.** Версия — **строка** `"1"`, не число. `format_version: 1` (без кавычек) тоже работает на чтение (ryml даёт `val()` "1"), но писать всегда в кавычках.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Edges как `[from_idx, to_idx]`, а не пары полных путей | Компактнее, и диффы по baseline стабильнее при перестановках строк |
| `format_version` строкой (`"1"`), не числом | YAML-числа имеют свои сюрпризы, строка проще |
| Один YAML-документ, не два файла | Простота для пользователя; v0.1 не нуждается в шардинге |
| Save вручную форматирует YAML, не гоняет ryml-emitter | Эмиттер ryml даёт мало гарантий по порядку/отступам; вручную дешевле и точнее |
| Error callback ryml кидает исключение, ловимое в `load_baseline` | Контракт ryml: callback не имеет права возвращаться; альтернативы — `setjmp/longjmp` или `abort` — несовместимы с C++ ресурс-менеджментом |
| `DependencyGraph` API не расширяли | `node_count + path_of + successors` уже хватает на полную итерацию; не плодим accessor'ы под одну задачу (YAGNI) |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `docs/baseline_format.md` | new (мини-спека формата) |
| `include/archcheck/graph/baseline.h` | new (публичный API) |
| `src/graph/baseline.cpp` | new (реализация на ryml) |
| `tests/unit/graph/baseline_test.cpp` | new (10 Catch2-кейсов) |
| `src/CMakeLists.txt` | подключён `graph/baseline.cpp` |
| `tests/CMakeLists.txt` | подключён `unit/graph/baseline_test.cpp` |
