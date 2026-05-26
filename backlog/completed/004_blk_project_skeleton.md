# [BUILD] Структура проекта и CMake-каркас

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** BUILD
**Приоритет:** blocker
**Сложность:** M (день)
**Блокирует:** #002 (github_actions_ci), #001 (dogfood_static_analyzers), все RULES/GRAPH/SCAN задачи
**Заблокирован:** ~~#003 (name_availability_check)~~ — разблокировано
**Related:** #006 (spec_refactor — roadmap v0.1 определил, что строим)

## Цель

Поднять минимальный C++20 CMake-каркас с layout из спеки, чтобы можно было начать писать первый модуль (`config/` или `scan/`).

## Контекст

Сейчас в репо нет ни одного `.cpp`, ни `CMakeLists.txt`. Спека (после #006) предписывает: C++20, CMake, **fast-backend по умолчанию (БЕЗ libclang в v0.1)**, минимум зависимостей (нет Boost), ryml + Catch2. Layout из CLAUDE.md: `src/{config,scan,graph,rules,report}/`.

## План выполнения

- [x] Верхний `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.18)` (под Astra 1.7 apt), C++20, warning-flags (`-Wall -Wextra -Wpedantic -Werror -Wshadow` / `/W4 /WX`)
- [x] Layout: `src/`, `include/archcheck/`, `tests/`. `fixtures/` и `third_party/` появятся позже.
- [x] YAML: **ryml** v0.7.0 через FetchContent
- [x] Test framework: **Catch2 v3** v3.7.1 через FetchContent
- [x] libclang: НЕ подключаем в v0.1 (fast backend по умолчанию per #006); запланировано в v0.2
- [x] Hello-world `archcheck --version` бинарь
- [x] Smoke-тест на константы версии
- [x] `.clang-format` (Allman, 3 пробела, 120 колонок)
- [x] `.clang-tidy` (bugprone-*, performance-*, modernize-*, cppcoreguidelines-*, readability-* без шумных чеков)
- [x] ~~`CMakePresets.json`~~ — выпил после реальной попытки: CMake 3.18 (Astra apt) не поддерживает presets (минимум 3.19). Используем явные `cmake -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`. Когда контрибьюторы появятся с свежим CMake — можно вернуть presets.
- [x] Обновить `CLAUDE.md` секцию "Build / test / run"

## Сделано

- **2026-05-26** — Каркас собран (commit `ea76db9`):
  - `CMakeLists.txt` верхний: C++20, strict warnings, FetchContent для ryml + Catch2.
  - `src/CMakeLists.txt`: `archcheck::core` (INTERFACE library) + `archcheck` (executable). Version macros через `target_compile_definitions`.
  - `src/main.cpp`: парсинг `--version` / `--help`, exit code 2 на unknown argument.
  - `include/archcheck/version.h`: `kVersionMajor/Minor/Patch` + `kVersionString` через preprocessor macros от CMake.
  - `tests/CMakeLists.txt` + `tests/smoke_test.cpp`: Catch2 + `catch_discover_tests`.
  - `.clang-format`: Allman, IndentWidth 3, ColumnLimit 120, IncludeBlocks Regroup.
  - `.clang-tidy`: 5 семейств чеков, шумные выключены явно.
  - `CMakePresets.json` v3: debug + release presets с Ninja, debug включает тесты, release — нет.
- **2026-05-26** — CLAUDE.md обновлён: секция «Build / test / run» с командами и описанием layout.
- **2026-05-26** — **Сборка прогнана и проверена** на Astra Linux 1.7 (gcc, ninja 1.11.1, cmake 3.18.4):
  - `cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug` — конфигурация прошла, ryml + Catch2 стянуты в `build/_deps/`.
  - `cmake --build build/debug` — 138 целей собрались без warning-ов на нашем коде (с `-Werror -Wshadow`).
  - `./build/debug/src/archcheck --version` → `archcheck 0.1.0` ✓
  - `./build/debug/src/archcheck --help` → usage ✓
  - `./build/debug/src/archcheck broken` → exit code **2** + usage ✓
  - `ctest --output-on-failure`: 2/2 PASSED (5 assertions).
  - В процессе верификации: убран `CMakePresets.json` (несовместимо с CMake 3.18), `cmake_minimum_required` понижен с 3.21 до 3.18, CLAUDE.md «Build» секция переписана под явные cmake-команды.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| YAML: **ryml** v0.7.0 | header-only-ish, low-allocation, без Boost — ложится на «single static binary». Спека листит первой. |
| Test: **Catch2 v3** v3.7.1 | Простая интеграция, эргономика для fixture-ориентированных тестов, нет необходимости в gmock. |
| Deps: **FetchContent** | Стандарт для OSS без vcpkg/conan. Первый билд online, дальше offline. CI кэширует `build/_deps/`. |
| Generator: **Ninja** в presets | Быстрее Make, доступен на всех платформах. |
| Сборка: Debug по умолчанию | CLAUDE.md: «не запускать Release без явной просьбы». ARCHCHECK_BUILD_TESTS=ON в debug, OFF в release. |
| `archcheck::core` как INTERFACE | В v0.1 нет реализации; subsystems (config/scan/graph/rules/report) подключаются по мере прихода. |
| `cmake_minimum_required` 3.21 | `CMakePresets.json` v3 требует 3.21. 3.20 не даёт presets v3. |
| Версия проекта = 0.1.0 | Согласуется с roadmap v0.1 в спеке. SemVer pre-1.0. |
| libclang НЕ подключаем | По #006 — fast backend = default в v0.1. libclang приходит в v0.2 через `--with-clang`. |

## Изменённые файлы

| Файл | Изменение | Commit |
|------|-----------|--------|
| `CMakeLists.txt` | новый — top-level, C++20, FetchContent ryml | `ea76db9` |
| `src/CMakeLists.txt` | новый — archcheck_core + archcheck binary | `ea76db9` |
| `src/main.cpp` | новый — hello-world с `--version` / `--help` | `ea76db9` |
| `include/archcheck/version.h` | новый — version constants | `ea76db9` |
| `tests/CMakeLists.txt` | новый — Catch2 + catch_discover_tests | `ea76db9` |
| `tests/smoke_test.cpp` | новый — 2 smoke-теста на версию | `ea76db9` |
| `.clang-format` | новый — Allman, 3 spaces, 120 cols | `ea76db9` |
| `.clang-tidy` | новый — narrow starter set | `ea76db9` |
| `CMakePresets.json` | новый — debug + release presets | `ea76db9` |
| `CLAUDE.md` | секция «Build / test / run» заполнена | (текущий) |
| `CHANGELOG.md` | запись про скелет | (текущий) |

## Как работает

CMake-конфигурация трёхуровневая:

1. **Top-level `CMakeLists.txt`** — задаёт project, C++20-стандарт, warning-флаги, опции (`ARCHCHECK_BUILD_TESTS`, `ARCHCHECK_ENABLE_WARNINGS`), подтягивает зависимости через FetchContent, делегирует в `src/` и `tests/`.

2. **`src/CMakeLists.txt`** — определяет два таргета:
   - `archcheck_core` (alias `archcheck::core`) — INTERFACE library, агрегирует include-paths, version macros, ryml. Подсистемы (`config/`, `scan/`, `graph/`, `rules/`, `report/`) будут подключаться сюда как PRIVATE источники через `target_sources()` по мере прихода соответствующих задач.
   - `archcheck` — executable, линкуется к `archcheck::core`. Сейчас только `main.cpp` с `--version` / `--help`.

3. **`tests/CMakeLists.txt`** — Catch2 v3 через FetchContent, `catch_discover_tests` регистрирует индивидуальные `TEST_CASE` в CTest.

**Зависимости стянутся при первом `cmake -B build`** в `build/_deps/`:
- `ryml-src` / `ryml-build` — YAML parser.
- `catch2-src` / `catch2-build` — test framework.

После первого билда обе live в кэше — повторные конфигурации offline.

**Version macros** прокидываются через `target_compile_definitions` на `archcheck_core`, заголовок `include/archcheck/version.h` читает их и оборачивает в `constexpr int / const char*`.

## Чем управляется

| Параметр | Где меняется |
|---|---|
| Версия проекта | `CMakeLists.txt` → `project(archcheck VERSION ...)`. Также фиксируется тегом `v0.1.0` при релизе. |
| Включить/выключить тесты | `cmake -DARCHCHECK_BUILD_TESTS=OFF` или preset `release` |
| Strict warnings | `-DARCHCHECK_ENABLE_WARNINGS=OFF` — отключает -Werror и компанию (для packaging downstream) |
| Build type | `--preset debug` или `--preset release`. Или классически `-DCMAKE_BUILD_TYPE=Debug` |
| Стиль кода | `.clang-format` — `clang-format -i src/*.cpp` |
| Статический анализ | `.clang-tidy` — `clang-tidy -p build/debug src/main.cpp` |

## С чем связана

- **#002 (github_actions_ci)** — теперь может стартовать. CI workflow ссылается на пресеты `debug`; добавит шаги `cmake --preset debug`, `cmake --build --preset debug`, `ctest --preset debug`, плюс `actions/cache` на `build/_deps/`.
- **#001 (dogfood_static_analyzers)** — теперь может стартовать. Будет вызывать clang-tidy / cppcheck / lizard на `src/` и `include/` (third_party-папки в `build/_deps/` фильтруются в `.clang-tidy`).
- **Будущие RULES/GRAPH/SCAN задачи** — подключают свои source-файлы в `archcheck::core` через `target_sources(archcheck_core PRIVATE ...)`.
- **#005 (sarif_reporter_spec)** — будет добавлять `src/report/sarif_reporter.{h,cpp}` (но это v0.2+).
- **#006 (spec_refactor)** — заложил roadmap v0.1, по которому этот каркас построен (fast-backend-first, без libclang).

## Диагностика

Команды для верификации (пока сборка не запущена пользователем):

```bash
# Конфигурация (требует интернет на первый запуск — стягивает ryml + Catch2):
cmake --preset debug

# Сборка:
cmake --build --preset debug

# Запуск тестов:
ctest --preset debug

# Запуск бинаря:
./build/debug/src/archcheck --version
# Ожидается: "archcheck 0.1.0"

./build/debug/src/archcheck --help
# Ожидается: usage с описанием available commands

./build/debug/src/archcheck unknown
# Ожидается: exit code 2 + сообщение об ошибке

# Проверить linter-конфиг:
clang-format --version    # >= 16
clang-tidy --version      # любой современный

# compile_commands.json должен быть на месте:
ls build/debug/compile_commands.json
```

Если первый билд падает на FetchContent — проверить:
- интернет есть?
- порт 443 на github.com доступен?
- `git --version >= 2.17`?
- (если корпоративный прокси) `https_proxy` / `http_proxy` выставлены?
