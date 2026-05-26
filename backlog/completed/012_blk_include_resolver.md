# [SCAN] Include resolver — zero-config repo-local

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** SCAN
**Приоритет:** blocker
**Сложность:** M (1-2 дня)
**Блокирует:** #015 (diff_primitives), #017 (graph_fixtures)
**Заблокирован:** #008g (include_scanner_macro_include_diagnostic), #011 (discover_files_and_indexes)
**Related:** #008 (dependency_graph_foundation)

## Цель

Превратить `IncludeDirective` + project indexes в `ResolvedInclude` одной из
четырёх категорий: `project` / `external` / `unresolved` / `ambiguous`.

## Контекст

См. §4 mini-design в #008. Это сердце zero-config extraction layer: именно
здесь принимается решение, попадает ли include в граф ребром.

## Сделано

- **2026-05-26** — создан `include/archcheck/scan/resolved_include.h` (`Resolution`, `ResolvedInclude`).
- **2026-05-26** — создан `include/archcheck/scan/include_resolver.h` (объявления `resolve_include` / `resolve_includes`).
- **2026-05-26** — реализован `src/scan/include_resolver.cpp` с алгоритмом quote/angle resolution.
- **2026-05-26** — добавлен unit-тест `tests/unit/scan/include_resolver_test.cpp` (12 кейсов: 5 веток quote, 4 angle, доп. tie-break + batch).
- **2026-05-26** — `archcheck_core` подхватывает новый `.cpp`, `archcheck_tests` — новый test source.
- **2026-05-26** — Debug-сборка зелёная, `ctest` 75/75, `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — чисто.

## Как работает

Public API сидит в `include/archcheck/scan/`:
- `Resolution` — `Project` / `External` / `Unresolved` / `Ambiguous`.
- `ResolvedInclude` — `{ directive, source_file, resolution, target, candidates }`.
  `target` валиден только при `Project`; `candidates` заполняется только при `Ambiguous`.
- `resolve_include(directive, source_file, files, index) -> ResolvedInclude` — единичный резолв.
- `resolve_includes(directives, source_file, files, index) -> std::vector<ResolvedInclude>` — батч.

Алгоритм (один проход, без backtracking):

**Quote `#include "x"`:**
1. Склеиваем `dir(source_file) + x` → exact match по `exact_path_index`.
2. Если нет — exact match по `x` напрямую.
3. Если нет — `suffix_index[x]`:
   - 0 кандидатов → `Unresolved`;
   - 1 кандидат → `Project`;
   - 2+ → `Ambiguous` (`candidates` = весь вектор).

**Angle `#include <x>`:**
1. Exact match по `x` через `exact_path_index`.
2. Если нет — `suffix_index[x]`:
   - 0 кандидатов → `External` (системный header — нормально);
   - 1 кандидат → `Project`;
   - 2+ → `Ambiguous`.

Дедупликация рёбер `(source, target)` сознательно НЕ делается на уровне
резолвера: это работа graph builder-а (#014), который и держит контракт
«один логический edge». Резолвер же должен сохранить все directives с их
`line`, чтобы позже можно было сообщить «дубль на line N».

## Чем управляется

- Никаких флагов / переменных среды.
- Поведение полностью детерминировано: `ProjectIndex` строится сортированно
  через `build_project_index`, suffix-кандидаты идут в том же порядке, что
  и id файлов в `discover_files` (тот сортирует по `path`).

## С чем связана

- Прямой потребитель — будущий graph builder (#014), который превратит
  `ResolvedInclude::Project` в ребро DAG, а остальные категории сложит в
  diagnostics.
- Использует `ProjectIndex::exact_path_index` и `suffix_index` из #011.
- Принимает `IncludeDirective` от scanner-а (#008a—#008g).
- `files` параметр в API оставлен для симметрии и будущих расширений
  (например, обратный lookup `NodeId -> ProjectFile`), но текущая
  реализация в нём не нуждается.

## Диагностика

- Если quote include неожиданно резолвится как `Unresolved`: проверить,
  что `source_file` передан как **repo-relative POSIX путь** (не absolute,
  не с `\\`), иначе `dir(source_file) + token` соберёт мусор.
- Если множество разных `.h` падают в `External`: возможно, файлы выпали
  из `discover_files` (исключены по имени директории или расширению —
  см. §1 mini-design в #008).
- `Ambiguous` — это не баг, а сигнал. В zero-config v0.1 namesake-файлы
  в разных директориях должны быть либо переименованы, либо разрешены
  явным include path (фича за пределами v0.1).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| `angle` без project-match → `external`, не `unresolved` | Соответствует обычной ожидаемой семантике system headers |
| Не пытаться различать `external` и «забытая зависимость» | Это работа отдельного будущего слоя (vendored / external manifest) |
| Дедупликация `(source, target)` отдана graph builder-у | Резолвер должен сохранить все directives с line; дедуп — свойство графа |
| `files` параметр принимается, но не используется | API-симметрия с будущими сценариями lookup-а; нулевая стоимость, явный сигнал «здесь будут files» |
| Реализация разбита на helpers (`source_directory`, `find_*`, `make_*`, `resolve_quote`, `resolve_angle`, `resolve_by_suffix`) | Lizard-пороги (≤ 30 строк / CCN ≤ 15) + читаемость; каждая функция отвечает за один шаг алгоритма |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/scan/resolved_include.h` | new |
| `include/archcheck/scan/include_resolver.h` | new |
| `src/scan/include_resolver.cpp` | new |
| `tests/unit/scan/include_resolver_test.cpp` | new (12 кейсов) |
| `src/CMakeLists.txt` | + `scan/include_resolver.cpp` |
| `tests/CMakeLists.txt` | + `unit/scan/include_resolver_test.cpp` |
