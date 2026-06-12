
## Как работает

`collectDiffAdvisories` суммирует added-строки уже собранного numstat
(`git::totalAddedLines`); при превышении `thresholds.diffMaxAddedLines`
complexity-advisory не вычисляется, в текстовый вывод идёт явная строка
«skipped — diff adds N lines (bulk import; thresholds.diff_max_added_lines)».
SATD и test-co-evolution не глушатся.

## Ключевые решения

- Дефолт 10 000: калибровка по корпусу #109 (честные фичи ≤ ~6К, дропы ≥ ~20К).
- Парсер thresholds переведён на таблицу member-pointer'ов (3 ключа, один путь).
- Бинарные файлы в numstat (added = −1) не уменьшают сумму (тест).

## Изменённые файлы

- `include/archcheck/config/config.h` — Thresholds.diffMaxAddedLines = 10000
- `src/config/config_loader.cpp` — ключ diff_max_added_lines, табличный парсер
- `include/archcheck/git/diff_query.h` — totalAddedLines()
- `src/cli/diff_command.cpp` — гейт + пометка skip + DiffConfig
- `fixtures/config/pass/thresholds/archcheck.yml`, тесты loader/diff_query
- `docs/config_format.md` — ключ задокументирован

## Итог

**Статус:** completed
**Дата завершения:** 2026-06-12
Проверено на реальном мега-дропе hpsx64 (+419 797 строк): advisory пропущен с
пометкой; обычный коммит (+36 строк) репортится как раньше.
