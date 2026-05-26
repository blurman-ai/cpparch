# [BUILD] Dogfood: cppcheck + clang-tidy + lizard в CI

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Статус:** wip
**Модуль:** BUILD
**Приоритет:** major
**Сложность:** S (день)
**Блокирует:** —
**Заблокирован:** ~~#002 (github_actions_ci)~~ — разблокировано
**Related:** #005 (sarif_reporter_spec)

## Цель

Не дать коду archcheck гнить — в каждом PR прогонять три дешёвые внешние тулзы поверх clang-tidy: **cppcheck**, **lizard**, и второй проход clang-tidy с расширенным набором. Полный отчёт от archcheck на самом себе — отдельной таской, когда правила появятся.

## Контекст

Из исследования 2026-05-26: cppcheck ловит то, что clang-tidy пропускает (UB, утечки на другом анализаторе), lizard — пороги сложности (CCN ≤ 15, функция ≤ 30 строк — совпадает с `docs/code_quality.md`). Оба бесплатные, быстрые, низкий шум. scan-build и `-fanalyzer` — позже, в nightly.

## План выполнения

- [x] Отдельный job `static-analysis` в `ci.yml`, параллельно сборке (один runner, ubuntu-24.04)
- [x] **cppcheck:** `cppcheck --enable=warning,performance,portability --inline-suppr --error-exitcode=1 src/ include/` — падает на находках. Локально на нашем коде: 0 находок.
- [x] **lizard:** `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` + grep на `warning:` / `!!!` — падает на превышении (lizard сам по себе всегда exit 0). Локально на нашем коде: 0 находок.
- [x] **clang-tidy второй проход** с `.clang-tidy-strict` (bugprone, performance, modernize, cppcoreguidelines, readability, cert, misc, hicpp, llvm). Запускается с `continue-on-error: true` — **warning-only**, копит baseline, не валит сборку.
- [x] Сохранять отчёты (`lizard-report.txt`, `clang-tidy-strict-report.txt`) как artifacts на 14 дней
- [x] **Не** включать IWYU — мы сами в этой нише
- [x] **Не** включать flawfinder — устаревает, для нас почти всё мимо

## Сделано

- **2026-05-26** — Завёл `.clang-tidy-strict` (cert-*, hicpp-*, misc-*, llvm-* поверх обычного набора). Шумные чеки (`llvm-header-guard`, `llvm-include-order`, `hicpp-signed-bitwise`, `cert-err58-cpp`, `cppcoreguidelines-pro-bounds-*`) выключены явно.
- **2026-05-26** — В `.github/workflows/ci.yml` добавлен job `static-analysis` (параллелен `build`, ubuntu-24.04):
  - apt install: `cppcheck`, `clang-tidy-18`, `g++-13`, `ninja-build`, `cmake`; pip install: `lizard`.
  - Cache `build/debug/_deps` по отдельному ключу `deps-Linux-static-...` (изолирован от build-cache, иначе race condition при параллельном write).
  - `cmake configure` для генерации `compile_commands.json` (нужен clang-tidy).
  - **cppcheck**: gate (`--error-exitcode=1`).
  - **lizard**: gate с grep-проверкой на `warning:`/`!!!` (lizard сам всегда exit 0).
  - **clang-tidy strict**: `continue-on-error: true`, печатает report в лог в группе `clang-tidy strict report`, копит baseline.
  - Upload `lizard-report.txt` + `clang-tidy-strict-report.txt` как artifact на 14 дней.
- **2026-05-26** — **Локальная верификация**: cppcheck + lizard прогнаны на `src/`, `include/`, `tests/` — оба зелёные (exit 0, нет находок).
  - clang-tidy локально не проверен — машина имеет clang-tidy 11, который не поддерживает `--config-file=` (это с 12+). В CI используется clang-tidy-18.

## В работе
- (пусто)

## Следующие шаги

1. cppcheck job — самый дешёвый, начать с него
2. lizard job
3. clang-tidy strict как warning-only

## Ключевые решения

| Решение | Причина |
|---------|---------|
| cppcheck + lizard как gate, clang-tidy strict как warning | Минимум шума на старте, иначе бросим |
| scan-build/-fanalyzer — позже | Дороже по времени, в nightly когда стабилизируется CI |

## Изменённые файлы

| Файл | Изменение | Commit |
|------|-----------|--------|
| `.clang-tidy-strict` | новый — bugprone + performance + modernize + cppcoreguidelines + readability + cert + misc + hicpp + llvm | pending |
| `.github/workflows/ci.yml` | новый job `static-analysis` параллельно с `build` | pending |
