# [REPORT][DOGFOOD] Нашли своим же тулом: вынести `jsonEscape` в общий helper

**Дата создания:** 2026-05-30
**Дата старта:** 2026-05-30
**Дата завершения:** 2026-05-30
**Статус:** completed
**Модуль:** REPORT
**Приоритет:** minor
**Сложность:** small
**Блокирует:** —
**Заблокирован:** —
**Related:** #053 (fast_backend_line_duplication_pass — line-dup спайк, который нашёл дубль), #051 (config_loader_v1 — baseline уже в продукте, значит дедуп нужен в shipped code)

## Цель

Убрать реальный внутренний copy-paste в `archcheck::report`: одинаковая
`jsonEscape(const std::string&)` сейчас живёт и в
[src/report/json_reporter.cpp](../../src/report/json_reporter.cpp), и в
[src/report/violation_baseline.cpp](../../src/report/violation_baseline.cpp).
Вынести её в один общий helper и использовать это как первый чистый
**dogfooding-сюжет** для детектора дублей: «нашли своим же тулом, починили».

## История находки

Во время прогона standalone line-dup спайка по самому `cpparch` топовый
cross-file блок для репозитория оказался таким:

- `src/report/json_reporter.cpp:6`
- `src/report/violation_baseline.cpp:12`
- длина `21` значимая строка
- похожесть `100%`

Ручная проверка подтвердила, что это не test twin, не generated-код и не
packaging artifact. Это **реальный ручной дубль** в одном и том же модуле
`archcheck::report`: два файла держат одну и ту же минимальную JSON-экранизацию
для `"`, `\` и `\n`.

Это хороший учебный кейс для самого продукта:

1. fast line-dup слой нашёл небессмысленный дубль в собственном коде;
2. находка достаточно маленькая, чтобы чинить сразу;
3. после фикса можно честно говорить, что `archcheck` начал с себя.

## Контекст

Сейчас код выглядит так:

- [json_reporter.cpp](../../src/report/json_reporter.cpp) содержит локальную
  `jsonEscape()` в анонимном namespace и использует её при сериализации отчёта;
- [violation_baseline.cpp](../../src/report/violation_baseline.cpp) содержит
  вторую локальную `jsonEscape()` с тем же телом и использует её при сохранении
  baseline.

`jsonUnescape()` и парсинг baseline при этом нужны только
`violation_baseline.cpp`; их тащить в общий util не надо.

## Что сделать

- [x] Вынести общий helper `jsonEscape()` в небольшой report-level util.
- [x] Переключить `json_reporter.cpp` и `violation_baseline.cpp` на одну общую реализацию.
- [x] Оставить `jsonUnescape()` и baseline-specific parsing локальными, если у
      них нет второго пользователя.
- [x] Добавить тест на поведение helper-а: как минимум `"`, `\`, `\n`.
- [x] После фикса перегнать line-dup спайк на `cpparch` и убедиться, что этот
      конкретный блок больше не всплывает как top cross-file duplicate.

## Ограничения

- Не делать «мешок утилит» вроде `json_utils.cpp` без необходимости.
- Не тащить в задачу общий JSON-рефакторинг или смену формата baseline/report.
- Не менять поведение сериализации; задача только про дедупликацию реализации.

## Ожидаемое решение

Минимальный вариант:

- `include/archcheck/report/json_escape.h`
- `src/report/json_escape.cpp`

с одной функцией:

```cpp
std::string jsonEscape(const std::string& s);
```

Если при реализации выяснится, что `.cpp` не нужен и можно оставить
header-only helper без разрастания include-шума — это тоже допустимо. Главное:
**одна реализация, два пользователя, без лишней абстракции.**

## Критерий приёмки

- [x] В кодовой базе осталась ровно одна реализация `jsonEscape()`.
- [x] `json_reporter` и `violation_baseline` используют один и тот же helper.
- [x] Поведение не изменилось для `"`, `\`, `\n`.
- [x] Есть тест, который это фиксирует.
- [x] После фикса dogfooding-кейс задокументирован (см. «Итог»): пара
      `json_reporter ↔ violation_baseline` ушла из cross-file дублей `src/`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Чинить именно `jsonEscape`, а не весь JSON-слой | задача маленькая и подтверждена спайком |
| `jsonUnescape` не выносить заранее | второй пользователь отсутствует, YAGNI |
| Использовать кейс как dogfooding story | это сильный и честный сюжет для продукта |
| Не делать generic `json_utils` | один helper не оправдывает мешок абстракций |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/report/json_escape.h` | общий helper для JSON string escaping |
| `src/report/json_escape.cpp` | реализация helper-а (или header-only альтернатива) |
| `src/report/json_reporter.cpp` | убрать локальную копию, подключить helper |
| `src/report/violation_baseline.cpp` | убрать локальную копию, подключить helper |
| `tests/unit/report/...` | тест на `jsonEscape()` и/или affected roundtrip |

## Следующие шаги

1. Завести helper и переключить два call-site.
2. Добавить тест.
3. Перегнать спайк на `cpparch` и сохранить короткую заметку в completed-задаче
   как dogfooding-историю.

## Итог (2026-05-30)

**Сделано.** `jsonEscape()` вынесена в **header-only** helper
[include/archcheck/report/json_escape.h](../../include/archcheck/report/json_escape.h)
(`inline`, единственная зависимость — `<string>`; отдельный `.cpp` не заводили —
для крошечной чистой функции это лишний TU + запись в CMake). Оба call-site
([json_reporter.cpp](../../src/report/json_reporter.cpp),
[violation_baseline.cpp](../../src/report/violation_baseline.cpp)) переключены на
него; `jsonUnescape()` и baseline-парсинг остались локальными в
`violation_baseline.cpp` (второго пользователя нет → YAGNI). Тест:
[tests/unit/report/json_escape_test.cpp](../../tests/unit/report/json_escape_test.cpp)
(`"`, `\`, `\n` + комбинация). Сборка Debug зелёная, весь suite — 255 кейсов /
812 assertions pass. Ровно одно определение `jsonEscape` (в хидере).

**Dogfooding-результат.** Спайк (`experiments/line_duplication`, бинарь
`line_duplication`) собран и реально прогнан по `src/`:

```
$ line_duplication src --cross-only --min-lines 6 --top 12
duplicated blocks: 7   cross-file blocks: 5
top duplicated blocks:
  [CROSS-FILE] 10 lines  rules/sf7_using_namespace.cpp:83  <->  rules/sf8_include_guard.cpp:79
  [CROSS-FILE]  7 lines  graph/algorithms.cpp:9           <->  graph/baseline.cpp:13
  [CROSS-FILE]  7 lines  rules/lakos_god_headers.cpp:20   <->  rules/sf7_using_namespace.cpp:84
  [CROSS-FILE]  7 lines  rules/sf7_using_namespace.cpp:3  <->  rules/sf8_include_guard.cpp:3
  [CROSS-FILE]  6 lines  scan/include_resolver.cpp:4      <->  scan/include_scanner.cpp:5
```

Пара **`json_reporter.cpp ↔ violation_baseline.cpp` полностью ушла** из
cross-file дублей `src/` — исходный 21-строчный `jsonEscape`-блок исчез, и
никакого остаточного блока между этими двумя файлами спайк больше не показывает.
Критерий выполнен.

**Первая чистая dogfooding-история:** fast line-dup слой нашёл реальный ручной
дубль в собственном коде → починили → перегон подтвердил исчезновение. «Начали
с себя».

**Побочное наблюдение (вне скоупа #055).** В топе теперь всплывают другие
внутренние пары (sf7/sf8, graph algorithms/baseline и т.п.) — это в основном
boilerplate заголовков правил и include-блоки, не обязательно настоящий копипаст.
Отдельную задачу без команды не завожу.

## Как работает

`jsonEscape()` живёт одной `inline`-функцией в
`include/archcheck/report/json_escape.h` (зависимость — только `<string>`).
Оба report-писателя (`writeJsonReport`, `saveBaseline`) включают этот хидер и
зовут общий helper вместо локальных копий. Поведение байт-в-байт прежнее:
экранируются `"`, `\`, `\n`.

## Ключевые решения

- **Header-only `inline`, без `.cpp`** — крошечная чистая функция; отдельный TU
  и строка в CMake были бы лишним весом.
- **`jsonUnescape()` не выносили** — единственный пользователь
  (`violation_baseline.cpp`); выносить «на будущее» = нарушить YAGNI.
- **Дубль найден своим же line-dup спайком** — использован как первая честная
  dogfooding-история, а не просто рефактор.

## Изменённые файлы

- `include/archcheck/report/json_escape.h` — новый header-only helper `jsonEscape()` (commit b3c9594)
- `src/report/json_reporter.cpp` — убрал локальную копию, подключил helper (b3c9594)
- `src/report/violation_baseline.cpp` — убрал локальную копию, подключил helper (b3c9594)
- `tests/unit/report/json_escape_test.cpp` — новый тест на helper (b3c9594)
- `tests/CMakeLists.txt` — регистрация теста (b3c9594)

## Итог

**Статус:** completed
**Дата завершения:** 2026-05-30
**Коммит:** b3c9594
