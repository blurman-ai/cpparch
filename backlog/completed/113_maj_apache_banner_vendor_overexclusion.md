# [SCAN][GRAPH] Лицензионный баннер проекта ≠ вендор: убрать over-exclusion в графе

**Дата создания:** 2026-06-12
**Дата старта:** 2026-06-12
**Статус:** done
**Модуль:** SCAN][GRAPH
**Приоритет:** major
**Сложность:** small
**Исполнитель:** Haiku
**Блокирует:** — (мягко: будущее правило DRIFT.4)
**Заблокирован:** —
**Related:** #111 (находка §5.2 отчёта corpus run), #081 (прошлый over-exclusion: SPDX)

## Цель

Граф не должен терять собственные файлы проекта из-за того, что проект честно
ставит полный лицензионный баннер (Apache/MIT/BSD) в каждый свой файл.
Сейчас FDio/vpp (Apache-2.0, полный баннер в каждом файле до SPDX-миграции)
даёт граф **267 узлов из 2621 файла** — 90% проекта выкинуто как «вендор».

## Контекст

Корпусный прогон #111 (см.
[docs/research/lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md) §5.2)
вскрыл: `hasVendorLicenseHeader()` считает полный текст Apache-баннера
(маркер `"licensed under the apache"`) сигналом вендорённого файла. Эвристика
писалась для кейса «чужой файл с чужой лицензией среди наших», но у
Apache-лицензированных проектов баннер стоит в КАЖДОМ собственном файле.

Факты, проверенные живыми 2026-06-12:

- `include/archcheck/scan/file_classification.h` — `hasVendorLicenseHeader()`
  (~строка 190, массив `kLicenseMarkers` из 6 маркеров),
  `isVendoredFile()` (~строка 215) = name-layer ∨ license-layer.
- `src/graph/graph_builder.cpp` — `filterVendored()` (строки 55–74):
  единственное место, где license-layer влияет на **граф** (через
  `isVendoredFile(baseName, src)` на строке 66). Уже читает контент всех
  файлов в `FilteredFiles::contents`.
- Другие потребители `isVendoredFile`: `src/scan/project_files.cpp:162`
  (duplication-конвейер), `src/scan/test_co_evolution.cpp:38` (передаёт `""` —
  license-layer фактически выключен). Их поведение НЕ менять — вне скоупа.
- Тесты per-file функции: `tests/unit/scan/file_classification_test.cpp:53–78` —
  остаются зелёными без правок (саму функцию не меняем).
- Тестов на `graph_builder.cpp` нет; in-memory `FileSource`-стаб для образца:
  `FakeSource` в `tests/unit/scan/project_files_test.cpp:256`,
  `MapFileSource` в `tests/unit/scan/local_complexity_drift_test.cpp:13`.

## Решённый дизайн (развилок нет)

Чинить в `filterVendored()` (graph_builder.cpp), НЕ в `file_classification.h`:

1. Первый проход: для каждого файла, прошедшего dir/name-фильтры
   (`pathHasVendoredDir` / `pathHasTestDir` / `isTestBasename` /
   `isVendoredBasename` — name-layer оставить как есть), прочитать контент и
   посчитать `hasVendorLicenseHeader(src)`.
2. Если баннер у **> 50%** таких файлов — это лицензия проекта: license-layer
   на этом прогоне **отключается** (никого не выкидывать по баннеру).
   Иначе — текущее поведение (файлы с баннером выкидываются).
3. `isVendoredBasename` (qcustomplot/stb_/imgui и т.д.) действует всегда,
   независимо от доли — это name-layer.

Замечание по реализации: `filterVendored()` уже читает все контенты — собрать
тройки (file, content, banner-флаг) в один вектор первым проходом, решить по
доле, вторым проходом отфильтровать. Новых чтений с диска не добавлять.

## План выполнения

- [ ] Перестроить `filterVendored()` по дизайну выше (двухпроходный, та же
      сигнатура, ≤ 30 строк на функцию — при необходимости выделить helper
      в том же файле).
- [ ] Новый тест-файл `tests/unit/graph/graph_builder_test.cpp` (+ строка в
      `tests/CMakeLists.txt` — посмотреть, как подключены соседние
      `tests/unit/graph/*.cpp`). Стаб-FileSource скопировать по образцу
      `FakeSource` (project_files_test.cpp:256).
- [ ] Прогнать контрольные кейсы, build, tests, lizard, dogfood.

## Контрольные кейсы (контракт)

Все контенты в тестах начинать с баннера ПЕРВЫМИ байтами (функция смотрит
первые 2000 байт).

| Кейс | Вход | Ожидание |
|---|---|---|
| 1. Проектная лицензия | 4 файла `a.h b.h c.cpp d.cpp`, ВСЕ с `// Licensed under the Apache License, Version 2.0`, `d.cpp` включает `"a.h"` | nodes = **4**, edges = **1** |
| 2. Настоящий вендор | 5 файлов: 4 без баннера, 1 (`mini_lib.h`) с `/* Permission is hereby granted, free of charge */`; один из чистых включает `"mini_lib.h"` | nodes = **4**, edges = **0**, external+unresolved = 1 |
| 3. Ровно 50% | 4 файла, баннер у 2 | доля НЕ > 0.5 → баннерные выкинуты: nodes = **2** |
| 4. Name-layer при доминировании | 4 файла с Apache-баннером, один из них `qcustomplot.cpp` | nodes = **3** (qcustomplot выкинут именем, баннер-слой выключен) |

## Не делать

- НЕ менять `hasVendorLicenseHeader` / `isVendoredFile` / `kLicenseMarkers`
  в `file_classification.h` — per-file семантика остаётся.
- НЕ менять существующие ожидания в `file_classification_test.cpp`.
- НЕ трогать `project_files.cpp` / `test_co_evolution.cpp` (duplication-путь
  сознательно оставлен со старым поведением — отдельное решение, не гэп).
- НЕ добавлять конфиг-ручку/флаг для порога 50% — захардкоженная константа
  с комментарием-причиной.
- НЕ коммитить без команды.

## Definition of done

- 4/4 контрольных кейса зелёные с точными числами из таблицы.
- `cmake --build build/debug` + `ctest --output-on-failure` — всё зелёное.
- `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — 0 warnings.
- Dogfood: `./build/debug/src/archcheck` из корня — 0 нарушений на src/include/tests.

## Сделано

- `filterVendored()` переписан как двухпроходный. Добавлен `struct FilterEntry`
  (file, content, hasBanner) в анонимном namespace; `std::count_if` через
  `<algorithm>` (добавлен include).
- Первый проход: исключает по dir/test/basename-фильтрам, для остальных читает
  контент и фиксирует баннер-флаг в вектор кандидатов.
- Домиирование: `nBanner * 2 > candidates.size()` — строго > 50%.
- Второй проход: при доминировании все кандидаты проходят; иначе баннерные
  файлы исключаются (старое поведение). `isVendoredBasename` (name-layer)
  работает всегда, независимо от доли.
- Новый файл `tests/unit/graph/graph_builder_test.cpp`: 4 контрольных кейса,
  17 assertions; зарегистрирован в `tests/CMakeLists.txt`.
- 4/4 контрольных кейса из таблицы зелёные (nodeCount и edges точно).
- 506/506 тестов, lizard 0 warnings, dogfood 0 нарушений.
- Coverage: lines 91.5% / functions 96.5% / branches 57.6% — PASS.
- Коммит: `68437c0` (`fix(graph): project Apache banner ≠ vendor; two-pass filterVendored (#113)`)

## В работе

- (пусто)

## Следующие шаги

- (пусто)

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Доминирование баннера (>50%) = лицензия проекта | Конфиг-фри, ловит VPP-класс целиком; точечные маркеры не масштабируются |
| Фикс в graph_builder, не в file_classification | Per-file функция корректна для своего вопроса; ошибочна только интерпретация на уровне всего проекта |
| Duplication-путь не трогаем | Калибровался отдельно (#053–#059); смешивать семантики в одном изменении нельзя |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/graph/graph_builder.cpp` | `filterVendored()` → двухпроходный с порогом доминирования (commit `68437c0`) |
| `tests/unit/graph/graph_builder_test.cpp` | новый: 4 контрольных кейса (commit `68437c0`) |
| `tests/CMakeLists.txt` | + новый тест-файл (commit `68437c0`) |

## Как работает

Первый проход собирает вектор `FilterEntry{file, content, hasBanner}` для всех файлов,
прошедших dir/test/basename-фильтры. Затем считается доля: если `nBanner * 2 >
candidates.size()` (строго > 50%) — баннер — это лицензия самого проекта, banner-layer
выключается. Иначе — старое поведение (баннерные файлы = вендор). `isVendoredBasename`
(qcustomplot, stb_, imgui и т.д.) применяется до подсчёта баннеров и не зависит от доли.
`hasVendorLicenseHeader` / `isVendoredFile` в `file_classification.h` не тронуты —
per-file семантика корректна, ошибочна была только интерпретация на уровне всего проекта.

## Дата завершения

2026-06-12
