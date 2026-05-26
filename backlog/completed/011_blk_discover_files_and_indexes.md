# [SCAN] Discover files + project indexes

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** SCAN
**Приоритет:** blocker
**Сложность:** S (< 1 дня)
**Блокирует:** #012 (include_resolver)
**Заблокирован:** —
**Related:** #008 (dependency_graph_foundation)

## Цель

Обойти project root и собрать набор project files, плюс построить
`exact_path_index` и `suffix_index`, которые потребуются resolver-у.

## Контекст

См. §1 и §4 mini-design в #008. Без discovery и индексов resolver вообще не
может работать. Эта подзадача — следующий слой над textual scanner (#008a…g),
но независимая от него: scanner работает с одним файлом, discovery — со всем
репо.

## Сделано

- **2026-05-26** — создан `include/archcheck/scan/project_files.h`: типы
  `ProjectFile`, `ProjectIndex`, `NodeId = std::size_t`, объявления
  `discover_files(root)` и `build_project_index(files)`.
- **2026-05-26** — реализован `src/scan/project_files.cpp`:
  `recursive_directory_iterator` с пропуском excluded директорий, фильтр по
  расширениям, POSIX-нормализация через `path::generic_string()`,
  детерминированная сортировка результата.
- **2026-05-26** — реализован `build_project_index`: exact-path map +
  suffix-map по `/`-сегментам (через инкрементальный поиск следующего `/`).
- **2026-05-26** — `src/CMakeLists.txt`: `scan/project_files.cpp` добавлен в
  список исходников `archcheck_core`.
- **2026-05-26** — `tests/CMakeLists.txt`: подключён
  `unit/scan/project_files_test.cpp`.
- **2026-05-26** — написан unit-тест на 12 кейсов: пустой каталог, полный
  набор расширений, исключаемые директории (`.git`, `build`, `cmake-build-*`,
  `out` и др.), отсутствие авто-исключения `third_party/`/`vendor/`,
  POSIX-нормализация, детерминированная сортировка, exact lookup, полный
  suffix-набор, multi-candidate (collision), single-segment path.
- **2026-05-26** — Debug-сборка зелёная, `ctest` 51/51 (39 старых + 12 новых).
- **2026-05-26** — `lizard --CCN 15 --length 30 --arguments 5 --warnings_only`
  чист.

## Как работает

`discover_files(root)`:
- Использует `std::filesystem::recursive_directory_iterator` с
  `disable_recursion_pending()` для пропуска excluded директорий.
- Имена excluded директорий: `.git`, `build`, `.cache`, `.idea`, `.vscode`,
  `out`, плюс любые с префиксом `cmake-build-`.
- Принимаемые расширения (12 штук, см. §1 mini-design в #008):
  `.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .tpp .inl .inc`.
- Пути нормализуются через `path::generic_string()` — `/` на всех платформах,
  включая Windows.
- Все ошибки идут через `std::error_code` (не бросаем исключений).
- Финальная сортировка по `path` — детерминизм для CI.

`build_project_index(files)`:
- `NodeId` — это просто индекс в `std::vector<ProjectFile>`, что даёт O(1)
  back-lookup в файлы без отдельной структуры.
- `exact_path_index`: full repo-relative POSIX path → id.
- `suffix_index`: каждый `/`-сегментный суффикс пути → список id-кандидатов
  (`a/b/c.h` → `{"a/b/c.h", "b/c.h", "c.h"}`). Коллизии (одинаковое имя файла
  в разных директориях) формируют multi-candidate vector — resolver сам
  решает, как с этим обходиться (см. §4 mini-design).

## Чем управляется

- Никаких флагов / переменных среды на этом этапе.
- Список расширений и список excluded-директорий — `constexpr` массивы в
  `.cpp`. Если потребуется конфигурация, выделяем в struct позже.

## С чем связана

- Прямой потребитель — будущий `include_resolver` (#012). Resolver получает
  `ProjectIndex` и резолвит `IncludeDirective` в `NodeId` либо
  `external`/`ambiguous`/`unresolved`.
- Параллельно с textual scanner (#008a…g): scanner работает per-file,
  discovery — per-repo. Между ними нет прямой зависимости в коде.
- `NodeId` намеренно введён локально (`std::size_t`) — когда появится
  полноценный graph module (#008), типаж можно поднять в общий header без
  изменения публичного API discovery.

## Диагностика

- Если в выдаче появляются файлы из `build/` или `.git/` — проверить, что имя
  каталога приходит в `entry.path().filename()` ровно (мы сравниваем
  POSIX-имя без trailing slash).
- Если на Windows в путях оказались `\` — проверить, что используется
  `generic_string()`, а не `string()`.
- Если порядок результата нестабилен — проверить, что финальный `std::sort`
  не был случайно удалён (без него ctest на CI может моргать).
- Suffix-index не содержит ключа `c.h` для файла `a/b/c.h`? Проверить, что
  цикл по `start` начинается с `0` и продвигается на `slash + 1`, не на
  `slash`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Suffix index по `/`-сегментам, не по подстрокам | Совпадает с тем, как `#include` работает — путь, а не произвольная подстрока |
| Без авто-исключения `third_party/` | См. §1 mini-design: эвристики тихо не зашиваем |
| `NodeId = std::size_t` локально в `scan/` | `graph/` модуль ещё не существует (#008); подняли тип, когда понадобится — без слома API |
| `ProjectIndex` как struct, а не два свободных билдера | Один объект — один lifetime, одна точка передачи в resolver; нет шанса забыть один из индексов |
| Детерминированная сортировка `discover_files` | Контракт «same input → same output» из spec; CI не должен моргать из-за порядка обхода FS |
| `recursive_directory_iterator` + `error_code` overload | Никаких исключений в горячем пути; ошибки FS не валят весь scan |
| POSIX через `generic_string()` | Один формат пути на всех платформах — индексы должны сравниваться побайтно |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/scan/project_files.h` | new |
| `src/scan/project_files.cpp` | new |
| `tests/unit/scan/project_files_test.cpp` | new |
| `src/CMakeLists.txt` | добавлен `scan/project_files.cpp` в `archcheck_core` |
| `tests/CMakeLists.txt` | подключён `unit/scan/project_files_test.cpp` |
