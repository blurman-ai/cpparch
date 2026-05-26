# [BUILD] Структура проекта и CMake-каркас

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** BUILD
**Приоритет:** blocker
**Сложность:** M (день)
**Блокирует:** github_actions_ci, dogfood_static_analyzers, все RULES/GRAPH/SCAN задачи
**Заблокирован:** name_availability_check
**Related:** —

## Цель

Поднять минимальный C++20 CMake-каркас с layout из спеки, чтобы можно было начать писать первый модуль (`config/` или `scan/`).

## Контекст

Сейчас в репо нет ни одного `.cpp`, ни `CMakeLists.txt`. Спека предписывает: C++20, CMake, статический бинарь, минимум зависимостей (нет Boost), libclang/ryml/Catch2-или-GTest. Layout из CLAUDE.md: `src/{config,scan,graph,rules,report}/`.

## План выполнения

- [ ] Верхний `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.20)`, C++20, warning-flags (`-Wall -Wextra -Wpedantic -Werror`)
- [ ] Layout: `src/`, `include/archcheck/`, `tests/`, `fixtures/`, `third_party/` (для submodules/FetchContent)
- [ ] Выбор и подключение YAML: **ryml** (быстрее, header-only-ish) vs **yaml-cpp** (популярнее). Решить, зафиксировать
- [ ] Выбор тест-фреймворка: **Catch2 v3** vs **GoogleTest**. Решить, зафиксировать
- [ ] libclang: подключение через `find_package(Clang)` — *не* в первом PR, а отдельной таской (include_scanner запускается без него)
- [ ] Hello-world `archcheck --version` бинарь, чтобы было что собирать
- [ ] Один dummy-тест, чтобы было что прогонять
- [ ] `.clang-format` (Allman, 3 пробела — см. `docs/code_style.md`)
- [ ] `.clang-tidy` — узкий стартовый набор (bugprone-*, performance-*, modernize-*, cppcoreguidelines-* без noisy чеков)
- [ ] `cmake --preset debug` / `release` через `CMakePresets.json`
- [ ] Обновить `CLAUDE.md` секцию "Build / test / run" — сейчас там TODO

## Сделано
- (пусто)

## В работе
- (пусто)

## Следующие шаги

1. Принять решение по YAML и test framework (зафиксировать в "Ключевые решения")
2. Скелет CMake + hello-world
3. .clang-format + .clang-tidy

## Ключевые решения

| Решение | Причина |
|---------|---------|
| YAML: ? | TODO |
| Test: ? | TODO |
| Сборка: Debug по умолчанию | CLAUDE.md: "не запускать Release без просьбы" |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| CMakeLists.txt | новый |
| CMakePresets.json | новый |
| .clang-format | новый |
| .clang-tidy | новый |
| src/main.cpp | hello-world |
| tests/smoke_test.cpp | dummy |
| CLAUDE.md | секция Build |
