# [CI][DEVX] CI-шаг `clang-format --dry-run --Werror` для защиты стиля

**Дата создания:** 2026-05-27
**Дата старта:** 2026-05-27
**Дата завершения:** 2026-05-27
**Статус:** completed
**Модуль:** CI, DEVX
**Приоритет:** minor
**Сложность:** XS (1-2 часа)
**Блокирует:** —
**Заблокирован:** —
**Related:** #019 (cpp_style_realign — финальный шаг плана упоминал этот future-task), #002 (github_actions_ci)

## Цель

Добавить в GitHub Actions workflow шаг `clang-format --dry-run --Werror`
по всем `src/`, `include/`, `tests/`. Без CI-проверки `.clang-format` остаётся
рекомендацией — со временем дрейфует. С dry-run-проверкой PR с
несоответствием стилю не вмёрджится.

## Контекст

В #019 переехали на LLVM-base + Allman + `IndentWidth: 2` + `ColumnLimit: 120`.
Весь существующий код переформатирован (commit `7be32d1`). Чтобы guide не
жил в вакууме, нужен CI-gate. Это явно отмечено в плане #019 как
future-task.

Локально у мейнтейнера был `clang-format-11` (Astra default через unversioned
`clang-format`). В Astra-репах доступны `clang-format-18` и `clang-format-19`.
В CI matrix `build` уже стоят `clang-18` + `clang-tidy-18`. Решено пиннуть
**clang-format-18**: matches CI clang-toolchain, нативен в ubuntu-24.04
(не нужно подключать LLVM apt repo), доступен и в Astra.

## План выполнения

- [x] Установить локально `clang-format-18` (apt-get из Astra-репов, версия `18.1.8 (9.astra6)`)
- [x] Прогнать `clang-format-18 --dry-run --Werror` — 12 violations в 4 файлах (дрейф clang-format 11→18 + длиннее camelCase после #019 step 3/3)
- [x] Применить `clang-format-18 -i` ко всем `src/`, `include/`, `tests/` — отдельный reformat-коммит `b5f9a1b` (no semantic changes); SHA в `.git-blame-ignore-revs`
- [x] Добавить job `format-check` в `.github/workflows/ci.yml` — параллельно build / static-analysis, runs-on:ubuntu-24.04
- [x] Обновить `docs/code_style.md` — версия `clang-format-18` пиннута явно + инструкция «как починить локально»

## Ключевые решения

| Решение | Причина |
|---------|---------|
| `clang-format-18` (не 19, не 11, не unversioned) | Matches CI clang-toolchain (`clang-18` + `clang-tidy-18` в build matrix); ubuntu-24.04 native (не нужен LLVM apt repo); доступен и в Astra-репах для локалки |
| Отдельный **job** `format-check`, не step в build | Падает быстро (~30 сек), не зависит от FetchContent/cache. Параллельный feedback: PR упадёт на формате не дожидаясь cppcheck / lizard / clang-tidy |
| `--Werror`, не warning-only | Стиль либо enforced, либо нет. Warning-режим — это «дрейф разрешён» |
| `find ... \| xargs`, не glob через shell | Robust к большому числу файлов и пробелам в путях |
| Reformat — отдельным коммитом до CI step'а | Без него CI стал бы красным с момента merge; reformat-коммит дешевле split-коммита с CI и формат-фиксами |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `.github/workflows/ci.yml` | + job `format-check` (clang-format-18 --dry-run --Werror) |
| `docs/code_style.md` | секция «Инструменты»: clang-format-18 пиннута явно + how-to-fix |
| `.git-blame-ignore-revs` | + SHA reformat-коммита `b5f9a1b` |
| `src/`, `include/`, `tests/` (5 файлов) | reformat-коммит `b5f9a1b` (выравнивание continuation-параметров + sort using-declarations) |
