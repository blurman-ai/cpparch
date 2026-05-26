# [BUILD] CI на GitHub Actions

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Статус:** wip
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

| Файл | Изменение |
|------|-----------|
| .github/workflows/ci.yml | новый |
| README.md | CI badge |
