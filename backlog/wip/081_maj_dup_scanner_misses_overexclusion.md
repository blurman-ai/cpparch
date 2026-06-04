# #081: Дуп-сканер молча пропускает файлы — 2 бага over-exclusion

**Дата создания:** 2026-06-04
**Дата старта:** 2026-06-04
**Статус:** wip
**Приоритет:** maj
**Тип:** [BUG][scan]

---

## Цель

Дуп-скан (`--duplication`) молча выдаёт «scanned 0 files» на репах с реальным кодом —
копипаст не находится вообще. Найдено при корпусной верификации (#079/#080): 4 репы из
317 просканили 0 файлов, плюс частичные недосчёты. Две независимые причины.

## Bug A — `discoverFiles` рвёт весь обход на первой ФС-ошибке

[src/scan/project_files.cpp](../../src/scan/project_files.cpp) `discoverFiles`:
```cpp
while (!ec && it != end) { ... it.increment(ec); }
```
`!ec` означает: любая ошибка на ОДНОЙ записи (битый/зацикленный симлинк, нет прав)
останавливает весь обход. `jjbudz_6502` имеет `_codeql_detected_source_root -> .`
(симлинк на себя) → `recursive_directory_iterator` зацикливается → `ec` → 0 файлов
(хотя 10 tracked `.cpp`).

**Фикс:** не прерывать обход на per-entry ошибке — сбрасывать `ec` и продолжать; не
следовать за директорными симлинками (или ловить петли). Опции:
`directory_options::skip_permission_denied`. Затрагивает и граф (тот же `discoverFiles`).

## Bug B — SPDX-заголовок классифицируется как «vendored»

[include/archcheck/scan/file_classification.h](../../include/archcheck/scan/file_classification.h)
`hasVendorLicenseHeader` метит любой permissive-license баннер как вендорный. Но
**SPDX-заголовки сейчас стандарт на авторском коде**, не только в вендоре. Комментарий в
файле («archcheck's own sources carry no per-file license header») не держится на корпусе.

`fuddlesworth_PlasmaZones` (KDE-проект) — `SPDX-License-Identifier: GPL-3.0` на **всех 401**
своих `.cpp` → все помечены vendored → 0 файлов. Так же `ajazz-control-center`, `limitless`.

**Это дизайн-решение:** как отличить вендор-с-лицензией от авторского-с-SPDX. Варианты:
1. убрать license-header эвристику, опираться на dir/basename (#068/#069 layer 1);
2. срабатывать только если license-header И вендорная локация/basename;
3. сузить markers (SPDX сам по себе — не сигнал вендора).

## Масштаб

- **Полный ноль:** 4 репы (PlasmaZones 0/401, jjbudz 0/10, ajazz, limitless).
- **Частичный недосчёт:** где часть файлов с SPDX (выборка 120 реп показала onnx-light
  43/376, SpecusGL 34/174 — часть может быть легит-вендор, проверять по месту).
- Эффект: dup-сигнал **занижен** over-exclusion'ом (противоположно инфляции от fork/vendor).

## Acceptance

- [x] Фикстура: репа с self-symlink → дуп-скан не падает в 0 (Bug A).
- [x] Фикстура: авторский файл с SPDX-GPL заголовком НЕ исключается как vendored (Bug B).
- [x] Re-scan «пострадавшей» репы → ненулевой результат (репы корпуса локально недоступны,
      проверено эквивалентной temp-репой настоящим бинарём — см. Решение).

## Решение (2026-06-04)

**Bug A — устойчивый обход** ([src/scan/project_files.cpp](../../src/scan/project_files.cpp) `discoverFiles`):
- `recursive_directory_iterator` теперь с `directory_options::skip_permission_denied` —
  один нечитаемый поддерев не валит весь обход.
- Цикл `while (it != end)` (без `!ec`); после каждого `increment(ec)` делаем `ec.clear()` —
  per-entry ошибка не прерывает обход.
- Новый хелпер `should_skip_dir`: не спускаться в **симлинк-директории** (любые) —
  self/loop симлинк (`_codeql_detected_source_root -> .`) больше не зацикливает итератор.
  Per-entry статусы читаем в локальный `item_ec`, не в общий `ec`.

**Bug B — SPDX больше не сигнал вендора** (стратегия: вариант 3, выбран пользователем)
([include/archcheck/scan/file_classification.h](../../include/archcheck/scan/file_classification.h)
`hasVendorLicenseHeader`): убран маркер `spdx-license-identifier` из `kLicenseMarkers`
(7→6). Остались только **полные verbatim-тексты** лицензий (MIT/BSD/public-domain/
Apache/Boost/zlib) — авторские проекты их per-file не вставляют, а пишут одну SPDX-строку.
Layer 1 (basename/stem) и layer 1-dir (#068) не тронуты.

**Проверка:** все 344 теста зелёные; lizard чист; temp-репа (self-symlink + 2 файла с
`SPDX-License-Identifier: GPL-3.0`) даёт `scanned 2 files` вместо багового `scanned 0 files`.

**Изменённые файлы:**
- `src/scan/project_files.cpp` — Bug A (skip_permission_denied + no-follow symlinks + no-halt).
- `include/archcheck/scan/file_classification.h` — Bug B (drop SPDX marker).
- `tests/unit/scan/project_files_test.cpp` — тест self-symlink не обнуляет обход.
- `tests/unit/scan/file_classification_test.cpp` — bare SPDX не вендор; layer-2 на full-text.
- `tests/integration/graph/vendor_exclude_test.cpp` — фикстура layer-2 на full-text MIT.

## Related
- `[[backlog/completed/069_maj_vendored_file_exclude.md]]` — vendor-exclude (источник Bug B).
- `[[backlog/new/070_maj_checker_fp_fix_proposals.md]]` — общий FP-трекинг.
- `experiments/CORPUS_SUMMARY_REPORT.md` §8.4 — находка и масштаб.
