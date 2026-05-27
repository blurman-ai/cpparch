# [CI][DEVX] CI-шаг `clang-format --dry-run --Werror` для защиты стиля

**Дата создания:** 2026-05-27
**Статус:** new
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

`clang-format-19` уже стоит в локалке у мейнтейнера (через clangd-19). Для
CI достаточно `apt install clang-format-19` в GitHub Actions runner-е (ubuntu).

## План выполнения

- [ ] Найти `.github/workflows/*.yml` (см. #002 — основной CI workflow)
- [ ] Добавить job `clang-format-check` (или step в существующий) со следующими шагами:
  ```yaml
  - name: Install clang-format-19
    run: sudo apt-get install -y clang-format-19
  - name: Check formatting
    run: |
      find src include tests -name '*.h' -o -name '*.cpp' | \
        xargs clang-format-19 --dry-run --Werror --style=file
  ```
- [ ] Локально прогнать ту же команду — должна вернуть 0 (если нет — починить или пересоздать reformat-коммит)
- [ ] Документировать в `docs/code_style.md` short блок «как починить если CI красный»: `clang-format-19 -i <file>`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| `clang-format-19`, не unversioned | Версия должна совпадать с локальной у мейнтейнера; разные мажорные версии иногда форматируют по-разному |
| `--Werror`, не warning | Стиль либо enforced, либо нет. Warning-режим = «дрейф разрешён» |
| `find ... | xargs`, не glob через shell | Robust к большому числу файлов и пробелам в путях |

## Изменённые файлы (план)

| Файл | Изменение |
|------|-----------|
| `.github/workflows/*.yml` | + job/step `clang-format-check` |
| `docs/code_style.md` | + короткая заметка «починить локально через clang-format-19 -i» |
