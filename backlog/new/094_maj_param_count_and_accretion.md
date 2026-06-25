# [CHEAP-DRIFT][SCAN] Parameter Count and Accretion

**Дата создания:** 2026-06-10
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN / DIFF / REPORT
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #030 (baseline_file_flag), #051 (config_loader_v1), #075 (trusted_diff_workflow), #089 (boolean_state_drift), #090 (bool_field_accretion — образец delta-first advisory), #093 (flag_argument)

## Цель

Поймать два дешёвых сигнала деградации интерфейса:

- слишком широкие сигнатуры;
- рост числа параметров у уже существующей функции.

Это proxy на "вместо нового типа/модели продолжаем дописывать аргументы".

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 2.
- Внутри самого репозитория уже есть жёсткий лимит на число аргументов через `lizard` в dogfood/static-analysis, но это **не** означает, что продукту нужен ещё один глобальный lint-style gate. Здесь речь про cheap drift heuristic для пользовательских репозиториев и прежде всего про delta/accretion.
- Absolute parameter count без delta-контекста быстро превращается в "ещё один style checker", что конфликтует с позиционированием archcheck. Поэтому gate по полному снимку по умолчанию исключён.
- Для accretion-сигнала текущий violation baseline недостаточен как единственный механизм: нужно видеть "было N, стало M". Значит, первый практически полезный режим — сравнение с git ref / diff, а не попытка насильно уложить всё в существующий baseline violations list.

## План выполнения

### Detection contract

- `ARG.3.long_parameter_list`:
  - finding на новых/изменённых сигнатурах с числом параметров выше порога;
  - дефолтный порог ориентировочно `> 4`, но check остаётся advisory-first.
- `ARG.4.parameter_accretion`:
  - finding, если существующая функция прибавила `>= 2` параметра относительно базовой ревизии;
  - для gate-кандидата интересен именно рост на changed symbol, а не любой "давно плохой" long list.

### Matching strategy

- Переиспользовать token scan substrate из #093, если он уже есть.
- Для каждой вероятной function declaration/definition собрать приближённый key:
  - путь;
  - имя функции;
  - enclosing class/namespace, если дёшево извлекается;
  - line anchor.
- Если точное соответствие между old/new не найдено, ambiguous match понижается до advisory или пропускается. Нельзя gate-ить на хрупком сопоставлении.

### Алгоритм

1. `SignatureInfo` собирает `paramCount` тем же разбором top-level запятых, что
   ARG.1 в #093 (одна функция-парсер на оба правила): глубина `()`/`<>`/`[]` = 0;
   `()` и `(void)` → 0; `...` (variadic) считать одним параметром.
2. `ARG.3.long_parameter_list`: full scan → findings на `paramCount > 4`
   (Core Guidelines I.23 «fewer than four»); advisory. В diff-режиме — фильтр
   по added lines (как в #093), это gate-кандидат.
3. `ARG.4.parameter_accretion`: old/new `SignatureInfo` по changed files
   (`GitObjectFileSource` vs `DiskFileSource`); матчинг по ключу
   `file + qualifiedName` c **fallback по убыванию строгости**:
   exact (имя+arity старого == нового → дельты нет, скип) → имя совпало,
   arity вырос (кандидат accretion) → несколько одноимённых: nearest startLine,
   confidence=low. Finding при `newArity - oldArity >= 2` и confidence != low.
4. Перегрузки: если в old есть ровно одна функция с именем N и в new ровно одна
   с именем N — матч уверенный даже при разном arity (это и есть accretion);
   если кандидатов несколько с обеих сторон — все пары low → пропуск.
5. Тест-кейсы матчинга: добавление перегрузки (НЕ accretion — old 1, new 2,
   совпадающая arity-пара существует → скип); рост 3→5 (finding); rename (скип).

### Scope discipline

- `void` не считать параметром.
- Default arguments учитывать приблизительно, но не строить полноценный declarator parser.
- Function pointers, lambdas в параметрах и macro-generated signatures допустимо частично игнорировать, если это резко упрощает реализацию и явно задокументировано.
- Первый проход ориентировать на `--diff` / base-ref comparison. Отдельный saved snapshot для symbol metadata — только если без него невозможно сделать полезный сигнал.

### Конкретный план в текущем коде

1. Не делать второй scanner: расширить shared `function_signature_scan` из #093, а не писать ещё один parser под `param_count`.
2. В `SignatureInfo` сразу предусмотреть поля, нужные именно для этой задачи:
   `qualifiedName`/`owner`, `line`, `paramCount`, `hasVoidParameterList`, `hasDefaultArgs`.
3. `ARG.3.long_parameter_list` считать на full scan через `collectNonVendoredSources()` и текущий `DiskFileSource`, без graph path и без `IRule`.
4. `ARG.4.parameter_accretion` строить через compare двух снимков по changed files:
   список файлов брать из `git::changedCppFiles()`;
   baseline side читать через `GitObjectFileSource` из [src/git/git_object_file_source.cpp](~/projects/cpparch/src/git/git_object_file_source.cpp);
   current side читать через `GitObjectFileSource` или `DiskFileSource` в зависимости от `WORKTREE`.
5. Matching не класть в baseline JSON v1:
   в текущем коде baseline хранит только `(ruleId, file, line)`, этого недостаточно для symbol accretion.
   Сопоставление делать локально в diff path: `file + function name + nearest old line`.
6. Если в одном файле несколько одноимённых overload-ов и match неоднозначен, не gate-ить:
   либо advisory finding, либо пропуск. Это надо зафиксировать прямо в реализации.
7. Тесты привязать к уже существующим entry points:
   parsing/counting — новый `tests/unit/scan/function_signature_scan_test.cpp`;
   accretion matching — новый `tests/unit/scan/parameter_accretion_test.cpp`;
   git/ref scenarios — расширение [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/param_count/pass/four_params.cpp`
- `fixtures/param_count/fail_long/five_params.cpp`
- `fixtures/param_count/fail_long/member_function.cpp`
- `fixtures/param_count/pass/void_param.cpp`
- `fixtures/param_count/pass/default_args.cpp`
- `fixtures/param_count/fail_accretion/baseline.cpp`
- `fixtures/param_count/fail_accretion/current.cpp`

Обязательные проверки:

- long parameter list детектится стабильно;
- `void` не считается отдельным аргументом;
- рост параметров между old/new фиксируется;
- ambiguous matching не становится gate;
- JSON/text output не ломает существующий contract;
- dogfood-прогон не даёт взрыва ложных срабатываний на текущем коде `archcheck`.

## Критерии готовности

- Absolute count остаётся advisory по умолчанию.
- Value check — в delta/accretion, не в наказании legacy whole tree.
- Никакого libclang.
- Никакого нового config top-level key.
- Описан trade-off между точностью сопоставления и простотой реализации.

## Не делать

- AST-level identity matching функций.
- Полный pretty-printer типов.
- Историческую аналитику по `git log` в рамках этой задачи.
- Универсальную symbol database "на будущее".

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Переиспользовать или выделить минимальный function-signature scanner.
2. Зафиксировать matching strategy old/new.
3. Реализовать long-count и accretion detectors.
4. Прогнать diff fixtures и dogfood.
5. Решить, нужен ли отдельный symbol snapshot, или хватает сравнения с git ref.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| В центре задачи `parameter accretion`, а не просто `max_params` | Это ближе к drift-модели и меньше конфликтует с "не линтер" |
| Ambiguous match не может gate-ить | Нельзя строить CI-fail на хрупком эвристическом сопоставлении |
| Reuse scanner из #093, если он уже существует | Минимум дублирования без новой абстракции |
| Saved baseline metadata не обязательна в v1 | У проекта уже есть полезный `--diff`, а schema baseline для symbol metrics пока нет |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/scan/` или `src/cheap_drift/` | Переиспользуемый signature scanner |
| `src/main.cpp` / orchestration | Сравнение current vs base ref |
| `tests/unit/` | Счётчик параметров и matching logic |
| `tests/integration/` | Diff/accretion сценарии |
| `fixtures/param_count/` | `pass/`, `fail_long/`, `fail_accretion/` |
