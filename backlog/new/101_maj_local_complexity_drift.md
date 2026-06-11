# [DRIFT][DIFF] Local Complexity Drift Advisory Signal

**Дата создания:** 2026-06-10
**Дата старта:** —
**Статус:** new
**Модуль:** GIT / SCAN / DIFF / REPORT
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #024 (in_memory_fs_for_diff), #075 (trusted_diff_workflow), #093 (flag_argument), #096 (satd_delta), #097 (test_co_evolution), #099 (indentation_complexity_drift)

## Цель

Добавить advisory-only drift signal, который ловит локальный рост control-flow сложности в изменённом C/C++ коде:

- рост ветвления;
- рост вложенности;
- рост глубоко вложенных строк;
- рост локальной "patch accumulation" сложности внутри функций.

Это supporting signal для review, а не архитектурный gate и не попытка превратить `archcheck` в general-purpose linter.

## Контекст

- Источник постановки уже лежит в репо: [docs/codex_task_local_complexity_drift.md](~/projects/cpparch/docs/codex_task_local_complexity_drift.md).
- В текущем коде `--diff` умеет только graph-vs-graph structural report через `RegressionReport` из [src/diff/regression_report.cpp](~/projects/cpparch/src/diff/regression_report.cpp). Локальные drift-сигналы туда пока не встроены.
- Текущий snapshot report contract (`rules::Violation` + [json_reporter.cpp](~/projects/cpparch/src/report/json_reporter.cpp)) слишком узкий для `old_score/new_score/delta_percent/symbol/start_line/end_line`, а `--diff` вообще не поддерживает `--format json`.
- Текущий config schema (`version/modules/rules/thresholds`) не умеет ни `rules.local_complexity_drift`, ни advisory mode knobs. Значит, YAML shape из исходной постановки нельзя обещать в первой версии без отдельной schema-задачи.
- В репо нет стандартного `--strict` механизма. Значит, первая реализация должна быть advisory-only и без изобретения нового strict mode.
- Задача **пересекается** с #099: `indentation_complexity_drift` — это узкий file-level proxy, а эта задача — более полезный function-aware combined signal. Не реализовывать их как два независимых продукта; `#099` должен стать fallback/subsignal или быть закрыт как absorbed-by-#101.
- **Ревью прототипа (#102) нашло дефекты исходной формулы** — разбор с живыми репро:
  [docs/research/local_complexity_drift_scorer_review.md](~/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md).
  Ключевой принцип, который формула обязана соблюдать (постановка владельца):
  **объём ≠ сложность** — добавление 50 плоских строк (даже с глубоким отступом
  от выравнивания) должно давать score-дельту 0; рост даёт только добавленная
  управляющая структура (новый if, вложение, ветвление). Секция «Scoring model»
  ниже переписана под этот принцип. Ресёрч формального измерения завершён —
  спека, валидация, токенная реализуемость и дизайн дельта-сигналов:
  [docs/research/cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md).

## План выполнения

### Detection contract

- Основная единица анализа: функция или function-like body.
- Если функция не распознана надёжно, допускается file-level fallback, но только как secondary signal.
- Основной rule id:
  - `DRIFT.LOCAL_COMPLEXITY`
- Finding возникает, когда на changed code выполнено одно из условий:
  - новая функция имеет score выше порога;
  - существующая функция выросла на достаточный `delta_score`;
  - существующая функция выросла по `delta_percent` при заметном `meaningful_loc_delta`;
  - file-level aggregate сильно вырос и может быть полезен как fallback.

### Metric shape

Per-function:

- `file`
- `symbol`
- `startLine`
- `endLine`
- `meaningfulLoc`
- `branchScore`
- `maxNestingDepth`
- `deepLinesCount`
- `indentComplexitySum`
- `localComplexityScore`

Per-file aggregate:

- `functionCount`
- `meaningfulLoc`
- `totalLocalComplexityScore`
- `maxFunctionScore`
- `changedFunctionCount`
- `changedFunctionsComplexityDelta`

### Scoring model

Инвариант формулы: **объём кода не повышает score**. 50 добавленных плоских строк
(вызовы, инициализация, данные, выровненные продолжения) = дельта 0; score растёт
только от управляющей структуры. Ориентир — спека Sonar Cognitive Complexity
(Campbell 2018; та же метрика у CMU MSR 2026 — корпусные числа будут сравнимы
с литературой), с задокументированными токен-упрощениями.

- Сначала удалить шум:
  - blank lines;
  - comments;
  - string/char literals;
  - preprocessor-only lines для branch scoring.
- Инкременты структуры (`+1 + текущая control-вложенность`):
  - `if`, `for`, `while`, `do-while` (один раз за цикл — `while` от `do` не считается отдельно),
    `switch` (одна структура; **`case`/`default` НЕ считаются** — спека и так их не
    включала, прототип #102 считал ошибочно), `catch`, `?:`.
- Инкременты без вложенности (`+1`):
  - `else` / `else if` (как одна ветка, не две);
  - серия одинаковых `&&` / `||` (**+1 за серию, не за каждый оператор**;
    `&&` сразу после идентификатора/`>` в декларационном контексте — rvalue-ссылка,
    не считать);
  - `goto`; `break`/`continue` **с меткой или из вложенного цикла** (обычные — 0,
    как у Sonar); `co_await` — наш осознанный +1 (вне спеки Sonar).
- Вложенность (`currentControlNestingDepth`) растёт **только** внутри
  control-структур и лямбд — не от произвольных скобок, не от отступов.
- Indent-метрики (`indentComplexitySum`, `maxIndentLevel`, `deepLinesCount`)
  считать **только как диагностические поля** в отчёте и как file-level fallback
  (территория #099). **В score они не входят** — выравнивание аргументов и
  табы делают их форматозависимыми (репро в scorer review).
- Итог: `localComplexityScore = branchScore` (cognitive-style).
- Пороги findings — иерархия из дизайн-дока (cognitive_complexity_delta_design.md §5):
  (1) функция пересекла абсолютный порог **25** (default Sonar C-family и clang-tidy);
  (2) `delta > 0` в функции, уже бывшей выше порога (CodeScene-паттерн);
  (3) `delta >= K` (старт K≈5, калибровать на корпусе #102) — мягкий warning.
  PR-агрегат = сумма положительных дельт; отрицательные репортить как улучшение.
  **Не делить на размер диффа** (возвращает корреляцию с объёмом — Gil & Lalouche).
  Абсолютный `delta >= 5` из прототипа на функции со score 2000+ срабатывает
  от любого патча — не использовать.

### Runtime shape

- Работать только на baseline/current text contents по changed C/C++ files.
- Никакого libclang, compile database, include graph или compiler invocation.
- Порядок запуска по смыслу:
  - после diff extraction;
  - до graph build;
  - до clone detection;
  - как early advisory signal.

### Output semantics

- По умолчанию только advisory.
- Сообщение должно говорить именно про proxy drift:
  - "grew local complexity from X to Y"
  - а не "has cognitive complexity N".
- V1 не должен ломать CI default exit code.

### Конкретный план в текущем коде

1. Переиспользовать или, если ещё не landed, создать shared text preprocessing слой:
   `include/archcheck/scan/text_scan.h` + `src/scan/text_scan.cpp`.
   База для него уже фактически сидит в [src/scan/include_scanner.cpp](~/projects/cpparch/src/scan/include_scanner.cpp): stripping comments/raw strings, identifier helpers, line mapping.
2. Не писать второй независимый function parser:
   если #093/#094 успели завести `function_signature_scan`, расширить его до function-body discovery;
   если нет — здесь завести `include/archcheck/scan/function_body_scan.h` + `src/scan/function_body_scan.cpp` с консервативным распознаванием definitions (`signature ... {`), исключая `if/for/while/switch/catch`.
3. Добавить `include/archcheck/scan/local_complexity_metrics.h` + `src/scan/local_complexity_metrics.cpp`:
   структуры `FunctionComplexityMetrics`, `FileComplexityMetrics`;
   scorer по очищенному тексту;
   brace-based nesting + indent proxy.
4. Добавить `include/archcheck/scan/local_complexity_drift.h` + `src/scan/local_complexity_drift.cpp`:
   сравнение old/new metrics;
   сопоставление функций по `file + symbol + line anchor`;
   file-level fallback, если symbol match неоднозначен.
5. Для changed-file prefilter использовать уже существующий `git::changedCppFiles()` из [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp). Это уже даёт правильные semantics для `a..b` и `a..WORKTREE`.
6. Old/new contents читать через уже shipped `GitObjectFileSource` и `DiskFileSource`:
   [src/git/git_object_file_source.cpp](~/projects/cpparch/src/git/git_object_file_source.cpp),
   [src/scan/disk_file_source.cpp](~/projects/cpparch/src/scan/disk_file_source.cpp).
7. Поскольку текущий `run_diff()` в [src/main.cpp](~/projects/cpparch/src/main.cpp) сразу идёт в `runDiffFullPath()`, его надо разрезать на явные стадии:
   - parse revspec / repo root;
   - fast-path no C/C++ changes;
   - early local-complexity advisory pass;
   - graph diff pass;
   - unified output.
8. Не протаскивать это через `IRule`:
   [include/archcheck/rules/i_rule.h](~/projects/cpparch/include/archcheck/rules/i_rule.h) сейчас жёстко привязан к `DependencyGraph` и `readFile()`, что плохо подходит для old/new comparison per changed file.
9. Отдельно закрыть текущий report-model gap:
   сейчас `rules::Violation` и `json_reporter.cpp` не умеют `symbol`, `endLine`, `oldScore`, `newScore`, `deltaPercent`.
   Для v1 есть два допустимых пути:
   - минимально расширить `Violation` обратно-совместимо и обновить [src/report/json_reporter.cpp](~/projects/cpparch/src/report/json_reporter.cpp) / [src/report/text_reporter.cpp](~/projects/cpparch/src/report/text_reporter.cpp);
   - либо завести отдельный diff-report writer, который обслужит сразу #096/#097/#101 и наконец даст `--diff` JSON.
   Предпочтителен второй путь, потому что текущий `--diff` и так живёт мимо snapshot reporter-а.
10. Если #096 не успеет первым вынести git exec/diff query helpers, эта задача должна absorb-нуть минимальный reusable слой:
    `include/archcheck/git/git_exec.h`, `src/git/git_exec.cpp`,
    а при необходимости и `git/diff_query.cpp`.
11. Тесты лучше класть в уже существующие точки:
    - `tests/unit/scan/local_complexity_metrics_test.cpp`
    - `tests/unit/scan/function_body_scan_test.cpp`
    - `tests/unit/scan/local_complexity_drift_test.cpp`
    - `tests/integration/diff/git_diff_test.cpp` для repo-level baseline/current scenarios
    - при появлении diff JSON writer-а — отдельный `tests/unit/diff/...` на output contract.

### Детальная инструкция реализации (v1) — алгоритм, функции, реюз

Целевая семантика и обоснования — [cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) (§4 таблица токенной реализуемости, §5 сигналы, §6 ядро). Ниже — пошаговый план.

**Шаг 0 — что взять готовое (ничего из этого не переписывать):**
- лексер `lex()` из `include/archcheck/scan/duplication/token_normalizer.h` — уже
  пропускает комментарии/строки/raw strings; нужен **raw-режим** (см. шаг 1);
- `extractFragments` (`fragmenter.h`) — референс паттерна «`)` + `{` = тело»;
- `git::changedCppFiles()` (`src/git/git_state.cpp`) — список изменённых файлов
  с семантикой `a..b` и `a..WORKTREE`;
- `GitObjectFileSource` / `DiskFileSource` — old/new содержимое;
- vendor/test-фильтры `collectNonVendoredSources()` + `file_classification.h`.

**Шаг 1 — `rawLex()`** (расширение token_normalizer, ~20 строк):
вариант `lex()`, который не нормализует identifiers/literals, а возвращает
`Token { kind, text, line }`. Весь дальнейший анализ — по токен-потоку, никакого
повторного text-парсинга и никаких отступов.

**Шаг 2 — `function_body_scan`**: `std::vector<FunctionSpan> discoverFunctions(tokens)`,
`FunctionSpan { qualifiedName, paramArity, startLine, endLine, bodyTokenRange }`.
Алгоритм: кандидат = идентификатор (с опц. `::`-цепочкой / `operator@` / `~Name`),
за которым `( … )` (баланс скобок), затем опц. `const/noexcept/override/&/&&/-> T`,
затем `{`; исключить control-слова перед `(`; `paramArity` = top-level запятые + 1
(0 для `()` и `(void)`); конец тела — matching brace. Конструкторы: после `)` может
идти `: init-list` до `{` — пропускать до `{` на той же глубине.

**Шаг 3 — ядро `computeCognitiveComplexity(bodyRange, funcName)`** —
однопроходный, два стека (полная семантика в дизайн-доке §1/§4):
1. `braceStack`: каждый `{` классифицировать — CONTROL (предыдущие значимые токены:
   `)` control-заголовка / `else` / `do` / `try` / лямбда-интро) или NEUTRAL
   (`class/struct/namespace/enum/union`, `=`, `return`, `,`, `(`, init-list);
   `nesting` = число CONTROL-фреймов (+ лямбда-фреймы).
2. `pendingBraceless`: control-заголовок без `{` → nesting+1 до ближайшего `;`
   на той же глубине скобок (иначе `for(...) if(...)` недосчитывает).
3. Инкременты:
   - `if`: предыдущий значимый `else` → **+1** (hybrid); иначе **+1+nesting**;
   - `else` (не перед `if`): **+1**;
   - `for` / `while` (кроме хвоста do-while — флаг в braceStack) / `switch` /
     `catch`: **+1+nesting**; `do`: **+1+nesting**;
   - `?` (есть парный `:` до `;`/`,` той же глубины; не `::`): **+1+nesting**;
   - `goto`: **+1**; `case`/`default`/`try`/ранний `return`/`break`/`continue`: **0**.
4. Логические серии: `lastOp`-стек по глубине `(`; на `&&`/`||`/`and`/`or`
   в **логическом контексте** (предыдущий значимый — идентификатор/литерал/`)`/`]`,
   что отсекает rvalue-`T&&`): если оператор ≠ `lastOp` текущей глубины → **+1**;
   `!` перед `(`, открытие `(`, `;`, `,` сбрасывают `lastOp`.
5. Опции (default **off**, для сопоставимости с clang-tidy): `#if/#ifdef/#elif`
   **+1+nesting** (для braceStack брать только первую ветку, как lizard);
   direct-рекурсия (`funcName(` в теле) **+1**.
Возврат: `{ score, structuralCount, maxNesting, logicalSeries }` — последние три
только диагностика в отчёт.

**Шаг 4 — дельта `compareComplexity(oldFns, newFns)`**:
ключ = `file + qualifiedName + paramArity`; коллизия → nearest `startLine`,
`confidence=low`; блэклист символов `TEST|TEST_F|TEST_P|TYPED_TEST|BENCHMARK|MOCK_METHOD`;
Δ = new − old (новая функция: Δ = score). Findings по иерархии §5 дизайн-дока:
`LCX.1 crossed_25` (порог Sonar C-family/clang-tidy), `LCX.2 grew_when_already_above`
(Δ>0 при old≥25), `LCX.3 delta_ge_k` (K=5, мягкий). Low-confidence матчи не дают
LCX.1/LCX.2. PR-агрегат = Σ положительных Δ; отрицательные — отдельной строкой
(improvement).

**Шаг 5 — wiring**: стадии `run_diff()` из плана выше (п.7); text-блок после
structural diff; JSON — через отдельный diff-writer (п.9, путь 2).

**Шаг 6 — тест-матрица** (каждая строка = unit-кейс; контрпримеры из
scorer_review обязательны как регрессии):

| Кейс | Ожидание |
|---|---|
| плоский switch на 8 case | **1** (в прототипе было 19) |
| `if{if{if{}}}` | 1+2+3 = **6** |
| `else if`-цепочка ×5 | if=1 + 4×1 = **5**; та же как `else { if` → 1+2+3+4+5 |
| `a && b && c` | **1**; `a && b || c` → **2** |
| `Item&& x`, `auto&& y`, `std::forward` | **0** |
| выровненные продолжения аргументов | **0** |
| `do {…} while(x);` | **1** |
| рефорсат условия на 5 строк | дельта **0** |
| лямбда с `if` внутри | if = **+2** (1+nesting от лямбды) |
| keywords в строках/комментах/raw strings | **0** |
| `goto` | +1; ранние `return/break/continue` | 0 |
| init-list таблица данных | **0** |
| braceless `for(...) if(...) x;` | if = **+2** |

`function_body_scan_test`: free function, inline-метод, `Cls::method` out-of-class,
`operator()`, перегрузки (различены arity), конструктор с init-list, шаблонная функция.
`local_complexity_drift_test`: матчинг по arity, rename → new (без finding),
TEST_F-блэклист, low-confidence без LCX.1/2.

**Definition of done:** все synthetic-кейсы #102 + таблица выше зелёные; ручная
сверка с clang-tidy (порог 0) на ~20 функциях реального кода — расхождение ≤2;
corpus re-run #102: switch-парсеры и TEST_F ушли из топа, 6/6 TP сохранились.

## Fixtures & Tests

- `fixtures/local_complexity_drift/pass/flat_branches.cpp`
- `fixtures/local_complexity_drift/pass/comments_and_strings.cpp`
- `fixtures/local_complexity_drift/pass/preprocessor_lines.cpp`
- `fixtures/local_complexity_drift/fail_growth/update_baseline.cpp`
- `fixtures/local_complexity_drift/fail_growth/update_current.cpp`
- `fixtures/local_complexity_drift/fail_new_complex/new_complex_function.cpp`
- `fixtures/local_complexity_drift/pass/harmless_change_baseline.cpp`
- `fixtures/local_complexity_drift/pass/harmless_change_current.cpp`

Обязательные проверки:

- nested branches score выше, чем flat branches;
- comments/strings не создают fake branch tokens;
- preprocessor lines не увеличивают branch score;
- growth baseline → current даёт finding;
- harmless change (`run();` → `run(); log();`) finding не даёт;
- output содержит `rule id`, `file`, `symbol` если найден, `old/new score`, `delta`, advisory semantics;
- default exit code не меняется на failure только из-за этого сигнала.

## Критерии готовности

- Сигнал считает proxy metrics только по changed C/C++ files.
- Есть baseline/current comparison.
- Есть advisory findings про существенный growth.
- Нет зависимости от compile DB, libclang, include graph или Sonar.
- Есть тесты на nesting vs flat, comments/strings, growth и harmless-change no-op.
- Документация прямо говорит, что это supporting signal, а не default architecture gate.
- Принято одно ясное решение по overlap с #099: folded/subsignal, а не параллельная дублирующая реализация.

## Не делать

- Байт-в-байт сходимость с Sonar/clang-tidy как цель (семантика спеки — ориентир,
  расхождение ≤2 пункта на функцию — норма; см. дизайн-док §4).
- Полный C++ parser.
- Общий project-wide complexity gate.
- Ownership/knowledge-risk analysis.
- Defect prediction.
- Generic quality-lint subsystem.
- Auto-refactoring suggestions.
- New strict mode.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Решить и зафиксировать: переиспользуем `function_signature_scan` из #093/#094 или создаём `function_body_scan` прямо здесь.
2. Вынести shared text preprocessing из `include_scanner.cpp`.
3. Реализовать function metrics scorer.
4. Реализовать baseline/current compare по changed files.
5. Закрыть diff output contract: text + JSON.
6. Добавить fixtures и repo-level integration tests.
7. После этого вернуться к #099 и либо сложить его в helper/subsignal, либо закрыть как duplicate.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Advisory-only | В репо нет `--strict`, а продукт не должен становиться linter-ом |
| Function-aware combined score, а не только indent proxy | Это ближе к исходной постановке и полезнее для reviewer-а |
| Запускать до graph build | Сигнал дешёвый и не зависит от include graph |
| Не тянуть через `IRule` | Нужен old/new diff compare, а текущий `IRule` snapshot-only |
| Overlap с #099 закрывать консолидацией, не параллельной реализацией | Иначе будет два почти одинаковых complexity-сигнала с разной семантикой |
| Предпочесть отдельный diff-report writer, а не ломать snapshot `Violation` вслепую | Текущий `--diff` уже живёт вне normal reporter path |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `include/archcheck/scan/text_scan.h`, `src/scan/text_scan.cpp` | Shared stripping/line-map helpers |
| `include/archcheck/scan/function_body_scan.h`, `src/scan/function_body_scan.cpp` | Lightweight function-boundary detection |
| `include/archcheck/scan/local_complexity_metrics.h`, `src/scan/local_complexity_metrics.cpp` | Per-function/per-file complexity metrics |
| `include/archcheck/scan/local_complexity_drift.h`, `src/scan/local_complexity_drift.cpp` | baseline/current comparison and findings |
| `src/main.cpp` | Early diff-stage orchestration before graph build |
| `src/diff/` или `src/report/` | Unified diff advisory text/json output for #096/#097/#101 |
| `tests/unit/scan/` | metrics/body-scan/drift unit tests |
| `tests/integration/diff/git_diff_test.cpp` | real git baseline/current scenarios |
| `fixtures/local_complexity_drift/` | `pass/` and `fail_*` fixtures |
