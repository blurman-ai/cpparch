# [BUILD] CI на GitHub Actions

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** BUILD
**Приоритет:** critical
**Сложность:** M (день)
**Блокирует:** #001 (dogfood_static_analyzers)
**Заблокирован:** ~~#004 (project_skeleton)~~ — разблокировано
**Related:** —

## Цель

Каждый push и PR собирает Debug на Linux gcc+clang и прогоняет тесты. Зелёный CI с первого реального коммита кода.

## План выполнения

- [x] `.github/workflows/ci.yml`: matrix Linux (ubuntu-24.04) × {gcc-13, clang-18}, Debug
- [x] Кэширование `build/debug/_deps/` по ключу `hashFiles('CMakeLists.txt', 'tests/CMakeLists.txt', 'src/CMakeLists.txt')` (CMakePresets.json убран в #004, ключ обновлён)
- [x] Шаги: configure → build → ctest --output-on-failure → smoke binary
- [x] Падать на warning'ах (флаги уже в CMakeLists через `-Werror`)
- [x] Status badge в README
- [x] **Не** добавлять macOS/Windows в первом PR — отдельной таской, когда Linux зелёный
- [x] **Не** добавлять Release build пока не появится релизный workflow
- [x] `concurrency`-блок: cancel-in-progress для PR/push в один ref (экономит actions-минуты)

## Сделано
- **2026-05-26** — `.github/workflows/ci.yml` создан. Триггер: push в master + PR в master. Matrix: `ubuntu-24.04 × {gcc-13, clang-18}`. Шаги: checkout → install (ninja-build, cmake, gcc-13 / clang-18) → cache `build/debug/_deps/` через actions/cache@v4 → cmake configure (Ninja, Debug) → cmake --build → ctest --output-on-failure → smoke бинаря (`--version`, `--help`, exit 2 на unknown). Концъюренси-группа `ci-${ref}` с cancel-in-progress для экономии минут на rapid push.
- **2026-05-26** — README получил CI badge (URL пока на `cpparch`, после ренейма репо на GitHub auto-redirect-нет).

## В работе
- (пусто)

## Следующие шаги

1. Минимальный yaml на gcc-13 only, убедиться что зелёный
2. Добавить clang-18 в matrix
3. Status badge

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Только Linux на старте | Спека: статический бинарь под Linux/mac/Win, но MVP — Linux. Кросс-платформа позже |
| Debug only | CLAUDE.md: Release не запускаем без просьбы |

## Изменённые файлы

| Файл | Изменение | Commit |
|------|-----------|--------|
| `.github/workflows/ci.yml` | новый — matrix Linux × {gcc-13, clang-18}, Debug | `f9a8628` |
| `.github/workflows/ci.yml` | bump actions/checkout@v4→v5, actions/cache@v4→v5 (Node 24) | `a09d148` |
| `README.md` | CI badge | `f9a8628` |

## Как работает

GitHub Actions workflow `.github/workflows/ci.yml`:

1. **Trigger:** `push` в master + `pull_request` в master.
2. **Concurrency:** `ci-${ref}` с `cancel-in-progress` — rapid push не жжёт лишние actions-минуты.
3. **Job `build`** на `ubuntu-24.04` в matrix `gcc-13` × `clang-18` (fail-fast выключен).
4. **Шаги:**
   - `actions/checkout@v5` — Node 24.
   - `apt-get install` — ninja-build, cmake, конкретный compiler из matrix.
   - `actions/cache@v5` — кэш `build/debug/_deps/` по ключу `deps-Linux-{compiler}-hashFiles(CMakeLists.txt + src/CMakeLists.txt + tests/CMakeLists.txt)`. Restore-keys prefix `deps-Linux-{compiler}-` ловит unrelated cache hits.
   - `cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug` — конфигурация. При cache hit ryml + Catch2 уже в `build/debug/_deps/`.
   - `cmake --build build/debug` — компиляция.
   - `ctest --output-on-failure` в `build/debug` — прогон Catch2-сьюта.
   - Smoke бинаря: `--version`, `--help`, exit 2 на unknown arg.
5. **Status badge** в README рисуется по URL `actions/workflows/ci.yml/badge.svg`.

## Чем управляется

- **Версии actions:** `actions/checkout@v5`, `actions/cache@v5` — оба Node 24.
- **Matrix:** matrix.include в workflow. Добавить компилятор — добавить запись с `compiler / cc / cxx / apt-pkg`.
- **Cache key:** при изменении `CMakeLists.txt`/`src/CMakeLists.txt`/`tests/CMakeLists.txt` кэш инвалидируется. Это правильное поведение: если изменилась версия зависимости — стянуть заново.
- **Платформа:** ubuntu-24.04. Кросс-платформа (macOS, Windows) — отдельной таской.

## С чем связана

- **#004 (project_skeleton)** — этот CI собирает то, что в #004. Если меняется CMake-каркас, может потребоваться правка workflow.
- **#001 (dogfood_static_analyzers)** — разблокирована. clang-tidy + cppcheck добавятся отдельным job-ом в этот же workflow.
- **#005 (sarif_reporter_spec)** — когда SARIF появится (v0.2), CI получит шаг `upload-sarif` для GitHub Code Scanning.
- **`v0.1.0` тег** — когда дойдёт до релиза, появится отдельный release-workflow с release-сборкой и публикацией бинарей.

## Диагностика

```bash
# Локальная воспроизводимость того, что делает CI:
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
cd build/debug && ctest --output-on-failure

# Прогон под clang вместо gcc (если оба установлены):
CC=clang CXX=clang++ cmake -B build/debug-clang -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Статус последних CI-runs:
# https://github.com/blurman-ai/cpparch/actions

# Если кэш сломался (редко) — bump cache-key через тривиальную правку CMakeLists.
```

Если CI красный на ubuntu-24.04, но зелёный локально на Astra 1.7 — обычно это:
- разница версий apt-cmake (Astra 3.18 vs Ubuntu 3.28) — оба должны проходить ≥ 3.18 minimum;
- разница версий gcc/clang (на CI gcc-13 / clang-18; локально может быть старее);
- platform-specific warning-и под `-Werror` (нужно фиксить или сужать `-W`).
