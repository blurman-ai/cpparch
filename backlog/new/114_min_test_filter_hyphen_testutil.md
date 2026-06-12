# [SCAN] Тест-фильтр: дефисные суффиксы (-test) и каталоги testutil/

**Дата создания:** 2026-06-12
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN
**Приоритет:** minor
**Сложность:** small
**Исполнитель:** Haiku
**Блокирует:** —
**Заблокирован:** —
**Related:** #111 (находка §3.2 отчёта corpus run), #070 (исходное тест-исключение)

## Цель

Закрыть две дыры тест-классификации, найденные корпусным прогоном #111
(apache/impala): basename `otel-test.cc` (дефисный суффикс) и каталог
`be/src/testutil/` проходят фильтр и попадают в граф/метрики как
production-код.

## Контекст

Факты, проверенные живыми 2026-06-12 в
`include/archcheck/scan/file_classification.h`:

- `kTestDirNames` (~строка 225): `{"test", "tests", "unittest", "unittests"}`;
  сегменты нормализуются `normalizeDirSegment` (lowercase, `_`/`-`/пробел
  вырезаются) — т.е. `unit_test/` уже ловится как `unittest`.
- `isTestBasename` (~строка 243): stem начинается с `test_` ИЛИ кончается на
  `_test` / `_tests` / `_spec` (массив `kTestStemSuffixes`).
  **GCC8-COMPAT**: сравнение через `compare()`, не `ends_with` — сохранить стиль.
- Потребители (поведение меняется у всех, это и есть цель): 
  `src/graph/graph_builder.cpp:60`, `src/scan/project_files.cpp:157`,
  `src/scan/god_file_growth.cpp:36,170`, `src/scan/defect_attractor.cpp:29`,
  `src/scan/test_co_evolution.cpp:42`.
- Тесты: `tests/unit/scan/file_classification_test.cpp` —
  `pathHasTestDir` (строка 142) и соседний `isTestBasename` TEST_CASE.

## Решённый дизайн (развилок нет)

1. `kTestDirNames` += `"testutil"`, `"testutils"` (нормализация сегмента
   уже схлопнет `test_util/`, `test-utils/` в эти формы; ВАЖНО: текущая
   нормализация также схлопывает `test-utils` → `testutils` — отдельных
   написаний в массив не добавлять). Размер массива в объявлении
   `std::array<std::string_view, N>` поднять соответственно.
2. `isTestBasename`: к `kTestStemSuffixes` добавить `"-test"`, `"-tests"`,
   `"-spec"`; к prefix-проверке `test_` добавить `test-`.
   НЕ добавлять голый суффикс `test` без разделителя — `contest.cc`,
   `attest.h`, `latest.h` обязаны остаться production.

## План выполнения

- [ ] Правка `file_classification.h` по дизайну (оба пункта).
- [ ] Дописать кейсы в `tests/unit/scan/file_classification_test.cpp`
      (в существующие TEST_CASE для `pathHasTestDir` / `isTestBasename`),
      существующие REQUIRE не менять.
- [ ] Прогнать контрольные кейсы, build, tests, lizard, dogfood.

## Контрольные кейсы (контракт)

| Вызов | Ожидание |
|---|---|
| `isTestBasename("otel-test.cc")` | **true** |
| `isTestBasename("unit-tests.cpp")` | **true** |
| `isTestBasename("test-main.cc")` | **true** |
| `isTestBasename("widget-spec.cpp")` | **true** |
| `isTestBasename("contest.cc")` | **false** |
| `isTestBasename("attest.h")` | **false** |
| `isTestBasename("latest.h")` | **false** |
| `pathHasTestDir("be/src/testutil/scoped-flag-setter.h")` | **true** |
| `pathHasTestDir("be/src/test_utils/foo.h")` | **true** |
| `pathHasTestDir("be/src/observe/span-manager.cc")` | **false** |
| `pathHasTestDir("src/testutility/foo.h")` | **false** (сегмент ≠ testutil/testutils) |

## Не делать

- НЕ менять существующие ожидания тестов — только добавлять новые REQUIRE.
- НЕ трогать vendor-классификацию (`kVendoredDirNames` и т.д.) — это #113.
- НЕ использовать `std::string_view::ends_with` — GCC8-COMPAT, см. комментарий
  в самой функции.
- НЕ коммитить без команды.

## Definition of done

- 11/11 контрольных кейсов зелёные.
- `cmake --build build/debug` + `ctest --output-on-failure` — всё зелёное
  (включая нетронутые старые кейсы classification-теста).
- `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — 0 warnings.
- Dogfood: `./build/debug/src/archcheck` из корня — 0 нарушений.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Стартовать по плану.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Только разделённые суффиксы (`-test`, не `*test`) | contest/attest/latest — FP-класс; разделитель обязателен |
| `testutil(s)` как dir-имя, не basename | impala-кейс — каталог; basename `testutil.cc` без суффикса остаётся production |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/scan/file_classification.h` | `kTestDirNames` +2, `kTestStemSuffixes` +3, prefix `test-` |
| `tests/unit/scan/file_classification_test.cpp` | +11 контрольных REQUIRE |
