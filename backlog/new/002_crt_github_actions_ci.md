# [BUILD] CI на GitHub Actions

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** BUILD
**Приоритет:** critical
**Сложность:** M (день)
**Блокирует:** dogfood_static_analyzers
**Заблокирован:** project_skeleton
**Related:** —

## Цель

Каждый push и PR собирает Debug на Linux gcc+clang и прогоняет тесты. Зелёный CI с первого реального коммита кода.

## План выполнения

- [ ] `.github/workflows/ci.yml`: matrix Linux (ubuntu-24.04) × {gcc-13, clang-18}, Debug
- [ ] Кэширование CMake build dir по ключу `${{ hashFiles('CMakeLists.txt', 'CMakePresets.json') }}`
- [ ] Шаги: configure → build → ctest --output-on-failure
- [ ] Падать на warning'ах (флаги уже в CMakeLists через `-Werror`)
- [ ] Status badge в README
- [ ] **Не** добавлять macOS/Windows в первом PR — отдельной таской, когда Linux зелёный
- [ ] **Не** добавлять Release build пока не появится релизный workflow

## Сделано
- (пусто)

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
