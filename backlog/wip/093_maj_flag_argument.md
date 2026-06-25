# [CHEAP-DRIFT][SCAN] Flag-Argument Heuristics

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-23
**Статус:** wip — `ARG.1.flag_argument_signature` (сигнатуры) зашиплен (c0f37db, в релизе 0.1.0); остаток = `ARG.2` (call sites: ≥2 `true`/`false`-литерала в одном вызове)
**Модуль:** SCAN / DIFF / REPORT
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #030 (baseline_file_flag), #051 (config_loader_v1), #075 (trusted_diff_workflow), #086 (drift2_cycle_default_gate), #089 (boolean_state_drift), #090 (bool_field_accretion — образец delta-first advisory на fast-бэкенде)

## Цель

Добавить дешёвую эвристику для обнаружения интерфейсного дрейфа:

- сигнатур с булевыми флагами;
- вызовов с несколькими литералами `true` / `false`.

Это нужен именно как early-warning сигнал "поведение протекает в интерфейс", а не как строгий semantic rule.

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 1.
- Boolean-**поля** структур уже покрыты shipped-правилом `DRIFT.BOOL_FIELD_ACCRETION` (#090, в 0.1.0). Эта задача — про boolean **флаг-аргументы функций**: ортогональный cheap-drift сигнал, не «расширение» bool-field-правила. Общая с #090 — только методология (delta-first advisory, токенный fast-бэкенд), не феномен.
- Текущий runtime `archcheck` строится вокруг graph-backed `IRule` и минимального `Violation { ruleId, file, line, message }`. Задача не должна ломать текущий `--format json` v1 и не должна тащить второй параллельный формат отчёта.
- Текущий config contract (`version / modules / rules / thresholds`) не допускает произвольный top-level `cheap_drift:`. Первая реализация должна жить на встроенных дефолтах; отдельное расширение схемы — отдельная задача.
- По позиционированию продукта это **не intrinsic default rule уровня SF/Lakos**, а cheap drift heuristic. Значит: advisory-first, delta-first, без zero-config gate по всему legacy дереву.

## План выполнения

### Detection contract

- Декларация/definition:
  - finding при `>= 2` параметрах типа `bool`;
  - finding при `1` `bool`-параметре с подозрительным именем (`enable*`, `disable*`, `use*`, `force*`, `skip*`, `with*`, `without*`, `no*` и т.п.).
- Call site:
  - finding при `>= 2` литералах `true` / `false` в одном вызове;
  - finding при соседних булевых литералах;
  - смесь булевых литералов с magic numbers допустима как дополнительный сигнал только если не ухудшает recall/precision на fixtures.

### Алгоритм детектора (токенный)

Работать по токен-потоку `rawLex()` (см. #101 шаг 1 — лексер уже пропускает
комментарии/строки/raw strings), не по тексту.

**Сигнатуры (`ARG.1`)** — поверх `discoverFunctions()` из shared
`function_signature_scan` (#101 шаг 2 описывает тот же детектор границ):
1. Для каждого `FunctionSpan` разобрать параметр-лист по top-level запятым
   (глубина `()`/`<>`/`[]` = 0).
2. Параметр считается bool-параметром, если его токены содержат `bool`
   (включая `const bool`, `bool*`/`bool&` НЕ считать — указатель/ссылка это
   out-параметр, другой паттерн).
3. `boolParamCount >= 2` → finding; `== 1` и имя параметра матчит
   `^(enable|disable|use|force|skip|with|without|no|is|allow)` (case-insensitive,
   snake/camel) → finding. Имя = последний идентификатор параметра.
4. Декларации без тела (`;` вместо `{`) тоже считать — флаг-аргумент виден
   уже в заголовке; дедуп по `qualifiedName+arity` (декларация vs определение —
   один finding).

**Call sites (`ARG.2`)**:
1. Кандидат вызова = идентификатор + `(` (предыдущий значимый токен НЕ из
   {`if`,`for`,`while`,`switch`,`catch`,`return` — return допустим, но это вызов});
   исключить control-слова по списку Noise control.
2. Внутри скобок до парной `)` считать top-level литералы `true`/`false`.
3. `>= 2` литералов ИЛИ два литерала подряд через запятую → finding на строке вызова.
4. Не заглядывать в вложенные вызовы отдельно — каждый `ident(` обрабатывается
   своим проходом (стек).

**Дельта-режим**: после полного скана файла оставить findings, чьи строки попали
в added lines диффа (`collectAddedLines()` из #096) — фильтр по результату, не
отдельный сканер.

### Runtime shape

- Реализовать как лёгкий text/token pass, без libclang.
- Если в процессе нужен общий helper для cheap token scan, он должен быть минимальным и сразу переиспользуемым в #094 и #095. Не строить "универсальный parser framework".
- Комментарии и строковые литералы убирать до анализа, но с сохранением line mapping.
- Интеграцию с diff делать поверх существующего git/diff слоя, а не через разрозненные shell-вызовы из разных мест.

### Noise control

- Явно исключить `if`, `for`, `while`, `switch`, `catch`.
- Function-pointer syntax, macro-heavy случаи и template edge cases не обязаны поддерживаться в первом проходе, если это резко раздувает scope.
- Полный scan — только advisory/report.
- Gate-кандидаты — только новые/изменённые строки.

### Reporting

- Базовые rule ids:
  - `ARG.1.flag_argument_signature`
  - `ARG.2.boolean_literal_call`
- Если текущего `Violation` достаточно, использовать его без расширения.
- Если расширение всё же понадобится, делать только обратно-совместимое и сразу полезное ещё минимум для одной cheap-drift задачи.

### Конкретный план в текущем коде

1. Не изобретать новый источник файлов: для full scan брать `scan::FileSource` и `collectNonVendoredSources()` из [src/scan/project_files.cpp](~/projects/cpparch/src/scan/project_files.cpp), потому что там уже централизованы vendor/test exclusions.
2. Вынести text-preprocess primitives из [src/scan/include_scanner.cpp](~/projects/cpparch/src/scan/include_scanner.cpp) в новый shared helper `include/archcheck/scan/text_scan.h` + `src/scan/text_scan.cpp`:
   comment stripping, raw-string blanking, line map, identifier helpers. Иначе #093/#094/#095/#099 будут копировать одну и ту же логику.
3. Добавить `include/archcheck/scan/function_signature_scan.h` + `src/scan/function_signature_scan.cpp` с приближённым `SignatureInfo`:
   `name`, `owner`, `line`, `paramCount`, `boolParamCount`, `boolParamNames`.
   Этот слой потом сразу переиспользует #094.
4. Поверх него добавить detector `src/scan/flag_argument_scan.cpp`, который:
   отдельно обходит сигнатуры;
   отдельно ищет call sites по строкам/скобкам в уже очищенном тексте;
   сразу возвращает `rules::ViolationList`.
5. Full-scan wiring делать в [src/main.cpp](~/projects/cpparch/src/main.cpp) внутри `run_check()`:
   после создания `DiskFileSource src(root)` и до `applyBaselineAndReport()`.
   Через `IRule` это протаскивать не нужно: текущий `IRule` жёстко graph-backed.
6. Delta wiring делать не через `RegressionReport`, а рядом с `run_diff()`:
   file prefilter брать из `git::changedCppFiles()` в [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp),
   line-level gating брать из будущего shared helper `git/diff_query.cpp` (тот же helper нужен #096).
7. Тестовую матрицу раскладывать так:
   low-level text preprocessing — новый `tests/unit/scan/text_scan_test.cpp`;
   signature extraction — новый `tests/unit/scan/function_signature_scan_test.cpp`;
   rule behavior — новый `tests/unit/scan/flag_argument_scan_test.cpp`;
   repo-level diff scenarios — расширение [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/flag_argument/pass/basic.cpp`
- `fixtures/flag_argument/pass/control_flow_not_call.cpp`
- `fixtures/flag_argument/fail_signature/two_bool_params.cpp`
- `fixtures/flag_argument/fail_signature/suspicious_single_bool.cpp`
- `fixtures/flag_argument/fail_call/multiple_bool_literals.cpp`
- `fixtures/flag_argument/fail_call/mixed_literals_and_magic.cpp`

Обязательные проверки:

- full scan находит signature findings;
- full scan находит call-site findings;
- `if (ok && bad) {}` не парсится как вызов;
- diff mode репортит только added/changed-line findings;
- baseline/legacy suppression не превращает rule в full-tree gate;
- JSON/text output содержит стабильные `ruleId`, `file`, `line`, `message`.

## Критерии готовности

- Без libclang и без compile-commands.
- Без нового top-level `cheap_drift:` в `.archcheck.yml`.
- Без второго, несовместимого JSON format.
- FP/FN профиль описан хотя бы на fixtures и одном dogfood-прогоне.
- По умолчанию правило не валит CI на legacy full scan.

## Не делать

- Полный C++ parser.
- Macro expansion.
- Межфайловое symbol resolution.
- Отдельный generic "rule engine" под cheap drift.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Зафиксировать минимальный token-scan contract и line mapping.
2. Реализовать detector для сигнатур и detector для call sites.
3. Подключить diff filtering.
4. Прогнать fixtures и dogfood на самом `archcheck`.
5. Решить, нужен ли вообще апгрейд `Violation`, или хватает текущего контракта.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Advisory-first, delta-first | Иначе check быстро скатывается в ложно-положительный "линтер по legacy" |
| Без `cheap_drift:` в текущей задаче | Текущий config schema этого не допускает |
| Общий scanner helper допускается, общий framework — нет | Нужен reuse для #094/#095, но без YAGNI-абстракции |
| Call-site и signature signal в одной задаче | Это один и тот же интерфейсный drift-семейство |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/scan/` или `src/cheap_drift/` | Лёгкий token/text scanner helper |
| `src/main.cpp` и/или отдельный orchestration-слой | Подключение pass-а к full/diff режимам |
| `src/report/` | Только если нужен обратно-совместимый апгрейд репортинга |
| `tests/unit/` | Тесты scanner/detector логики |
| `tests/integration/` | Diff-mode сценарии |
| `fixtures/flag_argument/` | `pass/` и `fail_*` наборы |
