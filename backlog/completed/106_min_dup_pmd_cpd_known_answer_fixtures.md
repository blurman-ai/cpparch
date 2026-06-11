# [DUPLICATION][TEST] PMD CPD testdata как known-answer фикстуры для токенового слоя

**Дата создания:** 2026-06-11
**Дата старта:** 2026-06-11
**Статус:** wip — подход пересмотрен (см. Прогресс)
**Модуль:** DUPLICATION / SCAN / TEST
**Приоритет:** minor
**Сложность:** S
**Блокирует:** доверие к selective normalization (#056/#059)
**Заблокирован:** —
**Related:** #053 (line), #056 (token), #059 (precision), #107 (external oracle cross-validation)

## Цель

Взять готовые маленькие C/C++-файлы из тест-ресурсов PMD CPD (BSD-лицензия) и
завести их как **known-answer фикстуры** для нашего токенового слоя и selective
normalization. Каждый файл PMD заточен под один сценарий лексера/нормализации и
идёт в паре с `.txt`-ожиданием — это ровно то, чем мы и так проверяем токен-проход,
только написанное и отревьюенное чужими руками.

## Контекст

Источник (BSD-3, можно копировать с атрибуцией):
`pmd-cpp/src/test/resources/net/sourceforge/pmd/lang/cpp/cpd/testdata/`
https://github.com/pmd/pmd/tree/main/pmd-cpp/src/test/resources/net/sourceforge/pmd/lang/cpp/cpd/testdata

Прямые попадания в наши режимы:
- `ignoreIdents.cpp`, `ignoreLiterals.cpp` — игнор идентификаторов/литералов
  (наш ключевой режим selective normalization, см. docs/duplication_architecture.md).
- `literals.cpp`, `listOfNumbers.cpp` (+ варианты `_ignored`, `_ignored_identifiers`)
  — нормализация литералов.
- `continuation*.cpp`, `multilineMacros.cpp`, `preprocessorDirectives.cpp` —
  склейки строк и препроцессор (трогает наш fast-backend).
- `specialComments.cpp`, `tabWidth.cpp`, `unicodeStrings.cpp`, `utf8-bom_*.cpp` —
  пограничные кейсы лексера.

PMD-ожидания (`.txt`) — это их формат токенов, НЕ наш. Их нельзя сравнивать байт-в-байт;
надо переразметить ожидание под наш токен-выход (или под clone-пары, где это уместно).

## Что сделать

1. Скопировать релевантные `.cpp` в `fixtures/duplication/normalization/` с файлом
   `ATTRIBUTION.md` (PMD, BSD-3, ссылка на коммит-источник).
2. Для каждого определить ожидаемое поведение НАШЕГО детектора (а не PMD):
   с нормализацией identifiers/literals две «структурно одинаковые, но разные имена»
   секции должны схлопываться в клон-пару; без — нет.
3. Завести Catch2-тест: прогон токенового слоя на фикстуре → проверка ожидаемых
   клон-пар / отсутствия таковых.
4. Зафиксировать расхождения (если наш лексер ведёт себя иначе на continuation /
   utf8-bom / multiline-macros) — это либо баг, либо осознанное отличие в задаче.

## Definition of Done

- Фикстуры лежат под `fixtures/duplication/`, с атрибуцией.
- Зелёный Catch2-тест, покрывающий минимум ignoreIdents / ignoreLiterals / literals.
- Расхождения с нашим лексером либо устранены, либо явно задокументированы.
- Не трогаем сам детектор без причины — задача про покрытие, не про рефакторинг.

## Прогресс (2026-06-11) — подход пересмотрен

Полный разбор: `experiments/clone_oracle_validation/FINDINGS.md`.

**Ключевой вывод: end-to-end (CLI `--duplication`) — НЕ годится для PMD-сниппетов.**
PMD testdata — это одиночные сниппеты, а наш детектор работает на гранулярности
функций внутри корпуса и keys candidacy на общих **редких токенах** (df≤4),
переживших нормализацию. Прогнали 6 синтетических фикстур (renamed/exact/литералы) —
ни одна не сработала, даже байт-идентичная пара. Корни (по чтению кода):
- `idf=log(N/df)` вырождается в 0 на корпусе из 2 фрагментов;
- candidacy требует общий редкий токен (`buildRareTokenIndex`/`findCandidatePairs`);
- нормализатор (`pushIdentifierToken`, keepCalls) схлопывает все не-call идентификаторы
  в generic `id`, оставляя редкими только имена вызовов и литералы.
=> generic-болванка (`alpha/beta`) корректно игнорируется (анти-FP), но и наша
known-answer фикстура не ловится.

**Пересмотренный план (вместо CLI end-to-end):**
1. Тестировать **компоненты напрямую** в Catch2: выход `lex()`-нормализатора на
   PMD `ignoreIdents/ignoreLiterals/literals` (наш токен-стрим vs ожидание,
   адаптированное из PMD `.txt`); `weightedJaccard` на руками собранных bag'ах.
2. Скопировать релевантные PMD `.cpp` в `fixtures/duplication/normalization/`
   с `ATTRIBUTION.md` (PMD, BSD-3).
3. Для end-to-end кейсов (если нужны) — строить реалистичный мини-корпус с
   distinctive именами вызовов, не 2-функциональные игрушки.

**Блокирующая зависимость:** перед построением suite разобрать аномалию из #107/§4
(точная копия с редким литералом молча срезается) — иначе «0 пар» ≠ «нет клонов».
Скачанные PMD-файлы и репро-фикстуры: `experiments/clone_oracle_validation/pmd/`.

## Обновление (2026-06-11, вечер) — блокер снят, подход подтверждён

- Блокирующая аномалия (#107/§4) **разрешена**: это P0.6 joint-floor tradeoff (line≥0.50
  по сырым строкам режет «тяжёлые» переименования), не баг. Значит «0 пар» теперь
  объяснимо, suite строить можно.
- Первый компонентный тест по пересмотренному подходу уже есть и зелёный:
  `tests/duplication_renamed_recall_test.cpp` (rename-blind токены vs raw-line floor).
  Это шаблон для остальных known-answer кейсов на базе PMD-сниппетов.
- Дальше: адаптировать PMD `ignoreIdents/ignoreLiterals/literals` под component-level
  проверки `lex()` (наш токен-стрим), скопировать в `fixtures/duplication/normalization/`
  с ATTRIBUTION (BSD-3).

## Как работает

Фикстуры — пары файлов, различающиеся ровно одной осью (значения литералов /
имена локалов / имя callee); компонентный тест гонит их через `lex()` и сравнивает
нормализованные seq: blind-оси обязаны совпасть, selective-ось (callee) — отличиться.

## Ключевые решения

- **Component-level вместо CLI end-to-end**: candidacy детектора (редкие токены,
  idf) съедает крошечные сниппеты — честная known-answer проверка живёт на уровне
  `lex()`, не бинаря.
- **PMD `.txt`-ожидания не переносились**: это формат ИХ токенизатора; переразметка
  под НАШУ семантику (seq-равенство/неравенство) — суть задачи.
- **Точный пин количества `lit` (13)** вместо `>=` — сильнее ловит регрессии лексера.

## Изменённые файлы

- `fixtures/duplication/normalization/` — 5 фикстур + ATTRIBUTION.md (commit 2ff4522)
- `tests/duplication_pmd_normalization_test.cpp` — 3 TEST_CASE (commit 2ff4522)
- `tests/duplication_renamed_recall_test.cpp` — пин P0.6 tradeoff (commit 1fbc9f4)

## Итог
**Статус:** completed
**Дата завершения:** 2026-06-11
