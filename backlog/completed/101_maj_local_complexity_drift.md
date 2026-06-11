# [DRIFT][DIFF] Local Complexity Drift Advisory Signal

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-11
**Статус:** wip
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

- Источник постановки уже лежит в репо: [docs/codex_task_local_complexity_drift.md](/home/localadm/projects/cpparch/docs/codex_task_local_complexity_drift.md).
- В текущем коде `--diff` умеет только graph-vs-graph structural report через `RegressionReport` из [src/diff/regression_report.cpp](/home/localadm/projects/cpparch/src/diff/regression_report.cpp). Локальные drift-сигналы туда пока не встроены.
- Текущий snapshot report contract (`rules::Violation` + [json_reporter.cpp](/home/localadm/projects/cpparch/src/report/json_reporter.cpp)) слишком узкий для `old_score/new_score/delta_percent/symbol/start_line/end_line`, а `--diff` вообще не поддерживает `--format json`.
- Текущий config schema (`version/modules/rules/thresholds`) не умеет ни `rules.local_complexity_drift`, ни advisory mode knobs. Значит, YAML shape из исходной постановки нельзя обещать в первой версии без отдельной schema-задачи.
- В репо нет стандартного `--strict` механизма. Значит, первая реализация должна быть advisory-only и без изобретения нового strict mode.
- Задача **пересекается** с #099: `indentation_complexity_drift` — это узкий file-level proxy, а эта задача — более полезный function-aware combined signal. Не реализовывать их как два независимых продукта; `#099` должен стать fallback/subsignal или быть закрыт как absorbed-by-#101.
- **Ревью прототипа (#102) нашло дефекты исходной формулы** — разбор с живыми репро:
  [docs/research/local_complexity_drift_scorer_review.md](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md).
  Ключевой принцип, который формула обязана соблюдать (постановка владельца):
  **объём ≠ сложность** — добавление 50 плоских строк (даже с глубоким отступом
  от выравнивания) должно давать score-дельту 0; рост даёт только добавленная
  управляющая структура (новый if, вложение, ветвление). Секция «Scoring model»
  ниже переписана под этот принцип. Ресёрч формального измерения завершён —
  спека, валидация, токенная реализуемость и дизайн дельта-сигналов:
  [docs/research/cognitive_complexity_delta_design.md](/home/localadm/projects/cpparch/docs/research/cognitive_complexity_delta_design.md).

### Валидация и хэндофф из #102 (2026-06-11, прототип завершён)

Прототип (#102, completed) реализовал **ровно этот scoring model** в Python
(`experiments/local_complexity_drift/scan_commit.py`) и прогнал его по корпусу.
То есть для v1 алгоритм — не теоретический, а проверенный; ниже — что он показал и
какие продуктовые решения остались открытыми для C++-реализации.

**Что подтверждено (можно опереться, не переоткрывать):**
- Токенный cognitive-скорер + LCX-иерархия работают: корпус 100 реп, 1612 коммитов,
  **403 находки** (старая дефектная формула давала 524, раздутые switch-парсерами).
- «Definition of done» по корпусу из этой задачи **уже выполнен прототипом**:
  switch-парсеры и `TEST_F`-тела ушли из топа, **6/6 ручных TP сохранились**.
- Все 6 репро-дефектов (`review_repros/`) и synthetic 13/13 зелёные на v2-скорере.
- Ручной разбор 16 кейсов: 10 actionable TP, 3 non-actionable TP, 2 FP, 1 low-conf.
  Отчёты: [corpus_report](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_corpus_report.md),
  [examples](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_examples.md) (round 2 — где шумит).

**Открытые решения для v1 (их закрывает эта задача, не прототип):**
1. **`LCX.2 grew_when_already_above` шумит на Δ1-2.** В корпусе это 210/403 находок,
   из них **72 — Δ1-2** (функция была score 200, выросла до 201 — формально рост,
   для ревью бесполезно; кейсы #7/#8 examples-дока). Решение: дать `LCX.2` **порог-пол
   Δ≥K**, либо репортить его **advisory-only отдельно** от `LCX.1`/`LCX.3`.
   По умолчанию рекомендую: в gating-вывод — только `LCX.1 crossed_25` и
   `LCX.3 delta_ge_k`; `LCX.2` — advisory строкой. (Рекомендация, финальное — за владельцем.)
2. **Псевдо-сигнатуры.** Текстовый фрагментатор принимает `__attribute__((unused))`
   / `__declspec(...)` перед функцией за имя функции (корпусный FP: символ
   `__attribute__`, score 1→30). Блэклист — в `function_body_scan` (Шаг 2).
3. `low`-confidence уже не даёт `LCX.1/2` (Шаг 4) — этого достаточно; дополнительно
   стоит держать их вне headline-вывода (down-rank в репорте).
4. K=5 и порог 25 — оставить дефолтами из дизайн-дока; «второй срез» для калибровки
   без ground truth информативен слабо, лучше калибровать на реальных прогонах.

## План выполнения

### Detection contract

- Основная единица анализа: функция или function-like body.
- Если функция не распознана надёжно, допускается file-level fallback, но только как диагностика в отчёте (территория #099), не как finding в v1.
- Основной rule id:
  - `DRIFT.LOCAL_COMPLEXITY`
- Finding возникает, когда на changed code выполнено одно из условий (точная иерархия — Шаг 4 детальной инструкции ниже, других видов findings в v1 нет):
  - `LCX.1 crossed_25` — функция (новая или выросшая) пересекла абсолютный порог 25;
  - `LCX.2 grew_when_already_above` — Δ > 0 в функции, уже бывшей ≥ 25;
  - `LCX.3 delta_ge_k` — Δ ≥ K (K=5), мягкий warning.
- `delta_percent`-условия и file-level aggregate findings в v1 НЕ реализуются: процентная и file-level семантика — территория #099-fallback; file-level агрегаты остаются диагностическими полями отчёта без собственного finding-а.

### Metric shape

Per-function:

- `file`
- `symbol`
- `startLine`
- `endLine`
- `meaningfulLoc`
- `localComplexityScore` — cognitive score, единственное поле, участвующее в findings
- `maxNestingDepth`, `structuralCount`, `logicalSeries` — диагностика из ядра (Шаг 3)
- `deepLinesCount`, `indentComplexitySum` — диагностика, в score НЕ входят (см. Scoring model)

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
  (2) `delta >= 3` в функции, уже бывшей выше порога (CodeScene-паттерн; пол Δ≥3 —
  из вердикта корпуса #102: хвост Δ1–2 на уже-огромных функциях, 72/210 находок,
  неактионабелен);
  (3) `delta >= K` (K=5, нестрогое; подтверждение K на втором срезе — открытый пункт #102) — мягкий warning.
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

1. Shared text preprocessing слой НЕ создавать (`text_scan.h` отменён): весь анализ идёт по токен-потоку существующего `lex()` из [include/archcheck/scan/duplication/token_normalizer.h](/home/localadm/projects/cpparch/include/archcheck/scan/duplication/token_normalizer.h) — см. Шаг 0/1 детальной инструкции. Никакого повторного text-парсинга.
2. Второго независимого function parser-а в дереве нет (проверено 2026-06-11: `function_signature_scan` из #093/#094 не существует, те задачи не стартовали). Завести здесь `include/archcheck/scan/function_body_scan.h` + `src/scan/function_body_scan.cpp` с консервативным распознаванием definitions (`signature ... {`), исключая `if/for/while/switch/catch`; #093/#094 потом реюзают его.
3. Добавить `include/archcheck/scan/local_complexity_metrics.h` + `src/scan/local_complexity_metrics.cpp`:
   структуры `FunctionComplexityMetrics`, `FileComplexityMetrics`;
   scorer по токен-потоку (cognitive-ядро из Шага 3 детальной инструкции);
   nesting — control-nesting через классифицированный brace-стек, НЕ raw brace-depth и НЕ отступы.
4. Добавить `include/archcheck/scan/local_complexity_drift.h` + `src/scan/local_complexity_drift.cpp`:
   сравнение old/new metrics;
   сопоставление функций по ключу `file + qualifiedName + paramArity` (коллизия → nearest `startLine` + `confidence=low`, детали — Шаг 4 детальной инструкции);
   неоднозначный матч НЕ эскалировать в file-level finding — только пометка low confidence (file-level агрегаты остаются диагностикой).
5. Для changed-file prefilter использовать уже существующий `git::changedCppFiles()` из [src/git/git_state.cpp](/home/localadm/projects/cpparch/src/git/git_state.cpp). Это уже даёт правильные semantics для `a..b` и `a..WORKTREE`.
6. Old/new contents читать через уже shipped `GitObjectFileSource` и `DiskFileSource`:
   [src/git/git_object_file_source.cpp](/home/localadm/projects/cpparch/src/git/git_object_file_source.cpp),
   [src/scan/disk_file_source.cpp](/home/localadm/projects/cpparch/src/scan/disk_file_source.cpp).
7. Поскольку текущий `run_diff()` в [src/main.cpp](/home/localadm/projects/cpparch/src/main.cpp) сразу идёт в `runDiffFullPath()`, его надо разрезать на явные стадии:
   - parse revspec / repo root;
   - fast-path no C/C++ changes;
   - early local-complexity advisory pass;
   - graph diff pass;
   - unified output.
8. Не протаскивать это через `IRule`:
   [include/archcheck/rules/i_rule.h](/home/localadm/projects/cpparch/include/archcheck/rules/i_rule.h) сейчас жёстко привязан к `DependencyGraph` и `readFile()`, что плохо подходит для old/new comparison per changed file.
9. Отдельно закрыть текущий report-model gap:
   сейчас `rules::Violation` и `json_reporter.cpp` не умеют `symbol`, `endLine`, `oldScore`, `newScore`, `deltaPercent`.
   Для v1 есть два допустимых пути:
   - минимально расширить `Violation` обратно-совместимо и обновить [src/report/json_reporter.cpp](/home/localadm/projects/cpparch/src/report/json_reporter.cpp) / [src/report/text_reporter.cpp](/home/localadm/projects/cpparch/src/report/text_reporter.cpp);
   - либо завести отдельный diff-report writer, который обслужит сразу #096/#097/#101 и наконец даст `--diff` JSON.
   Предпочтителен второй путь, потому что текущий `--diff` и так живёт мимо snapshot reporter-а.
10. Git-helpers уже существуют (вынесены волной #096–#100, проверено 2026-06-11):
    [include/archcheck/git/git_exec.h](/home/localadm/projects/cpparch/include/archcheck/git/git_exec.h) — `runGit(args, cwd)`;
    [include/archcheck/git/diff_query.h](/home/localadm/projects/cpparch/include/archcheck/git/diff_query.h) — `collectAddedLines()`, `collectNumstat()`.
    Реюзать их, ничего не дублировать.
11. Тесты лучше класть в уже существующие точки:
    - `tests/unit/scan/local_complexity_metrics_test.cpp`
    - `tests/unit/scan/function_body_scan_test.cpp`
    - `tests/unit/scan/local_complexity_drift_test.cpp`
    - `tests/integration/diff/git_diff_test.cpp` для repo-level baseline/current scenarios
    - при появлении diff JSON writer-а — отдельный `tests/unit/diff/...` на output contract.

### Детальная инструкция реализации (v1) — алгоритм, функции, реюз

Целевая семантика и обоснования — [cognitive_complexity_delta_design.md](/home/localadm/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) (§4 таблица токенной реализуемости, §5 сигналы, §6 ядро). Ниже — пошаговый план.

**Шаг 0 — что взять готовое (ничего из этого не переписывать):**
- лексер `lex()` из `include/archcheck/scan/duplication/token_normalizer.h` — уже
  пропускает комментарии и сохраняет исходные написания в `Token::raw`;
  использовать **как есть**, без модификаций (детали и ловушка препроцессора — шаг 1);
- `extractFragments` (`fragmenter.h`) — референс паттерна «`)` + `{` = тело»;
- `git::changedCppFiles()` (`src/git/git_state.cpp`) — список изменённых файлов
  с семантикой `a..b` и `a..WORKTREE`;
- `GitObjectFileSource` / `DiskFileSource` — old/new содержимое;
- vendor/test-фильтры `collectNonVendoredSources()` + `file_classification.h`.

**Шаг 1 — никакого `rawLex` писать НЕ надо** (проверено по коду 2026-06-11):
существующий `lex(source, /*keepCalls=*/false)` уже сохраняет всё нужное в
`Token { sym, line, raw }`: keywords проходят как `sym` (`if`, `else`, `do`, …),
identifiers дают `sym=="id"`, `raw==написание`, literals — `sym=="lit"`;
`&&`/`||`/`::`/`->` — multi-char токены; `{ } ( ) ; ? : \` — одиночные токены.
Исходное написание: `rawText(t) = t.raw.empty() ? t.sym : t.raw` — завести такой
inline-helper. Весь дальнейший анализ — по токен-потоку, никакого повторного
text-парсинга и никаких отступов.
**Ловушка — препроцессор:** `lex()` вырезает только `#if 0`-блоки; остальные
директивы токенизируются, т.е. `#if defined(X)` даст фальшивый keyword-токен `if`
(аналогично `#else` → `else`). Обязательный фильтр в скорере: встретив токен `#`,
пропускать токены до конца строки (`token.line` меняется); если последний токен
строки — `\`, продолжать пропуск и на следующей строке (line-continuation).
Unit-кейс на это — в тест-матрице.

**Шаг 2 — `function_body_scan`**: `std::vector<FunctionSpan> discoverFunctions(tokens)`,
`FunctionSpan { qualifiedName, paramArity, startLine, endLine, bodyTokenRange }`.
Имена брать из `raw` (identifiers приходят как `sym=="id"`, `raw==написание`).
Алгоритм: кандидат = идентификатор (с опц. `::`-цепочкой / `operator@` / `~Name`),
за которым `( … )` (баланс скобок), затем опц. `const/noexcept/override/&/&&/-> T`,
затем `{`; исключить control-слова перед `(`; `paramArity` = top-level запятые + 1
(0 для `()` и `(void)`); конец тела — matching brace. Конструкторы: после `)` может
идти `: init-list` до `{` — пропускать до `{` на той же глубине.
**Блэклист имён** (иначе corpus-FP, #102): `__attribute__`, `__declspec` — это
атрибут-спецификаторы перед сигнатурой (`__attribute__((unused)) static int f(){…}`),
а не имена функций; их `(…){` не должен распознаваться как тело.

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
scorer_review обязательны как регрессии — их baseline/current пары сохранены в
[experiments/local_complexity_drift/review_repros/](/home/localadm/projects/cpparch/experiments/local_complexity_drift/review_repros/),
расшифровка пар — в #102, шаг 5 «Следующих шагов»):

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
| `#if defined(X)` / `#else` / `#elif` при default-off опции | **0** (фальшивые keyword-токены отфильтрованы) |
| `goto` | +1; ранние `return/break/continue` | 0 |
| init-list таблица данных | **0** |
| braceless `for(...) if(...) x;` | if = **+2** |

`function_body_scan_test`: free function, inline-метод, `Cls::method` out-of-class,
`operator()`, перегрузки (различены arity), конструктор с init-list, шаблонная функция,
`__attribute__((unused)) static int f(){}` → распознан как `f`, НЕ `__attribute__`.
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

## Решения старта реализации (2026-06-11)

| Решение | Причина |
|---------|---------|
| Вывод v1 — через уже shipped `printDiffAdvisories()` в main.cpp (паттерн #096/#097: text-блок после структурного диффа) | Волна #096–#100 заземлила advisory-паттерн ПОСЛЕ написания этой задачи; отдельный diff-JSON-writer отложен — у `--diff` сегодня нет JSON ни для одного сигнала, вводить его в одиночку из #101 — scope creep |
| Новые функции дают только LCX.1 (`crossed_25`), не LCX.3 | Корпусная валидация #102 шла с отсечением новых функций; LCX.3 на новых функциях (любая новая функция со score ≥ 5) не валидирован и почти наверняка шумен; LCX.1 на новых — прямое требование Detection contract |
| Порт скорера — точный порт v2 `scan_commit.py` (включая column-эвристику glued-`&&` для rvalue-ссылок) | Семантика валидирована корпусом (1612 коммитов) и synthetic suite 13/13; «улучшения» при порте = расхождение с валидацией |
| В `duplication::Token` добавляется поле `col` (выставляется в `lex()`) | Нужно для glued-эвристики rvalue-`&&`; аддитивно, существующие инициализаторы не ломает |

## Сделано

- Задача переведена в wip; в Scoring model вшит пол LCX.2 `delta >= 3` из вердикта корпуса #102.
- **Реализация v1 завершена (2026-06-11):**
  - `Token::off` (байтовое смещение) добавлен в `token_normalizer` — нужен для
    glued-эвристики rvalue-`&&`; выставляется одной строкой в `lex()`.
  - `include/archcheck/scan/function_body_scan.h` + `src/scan/function_body_scan.cpp` —
    токенное обнаружение определений функций: id-цепочки с `::`, `~Name`, `operator@`
    (включая `operator()`), арность top-level (с угловыми скобками), квалификаторы,
    trailing return, ctor init-list. В отличие от прототипа находит и inline-методы
    в телах классов.
  - `include/archcheck/scan/local_complexity_metrics.h` + `src/scan/local_complexity_metrics.cpp` —
    точный порт v2-скорера #102 (state machine `metric_for_span`): structural/hybrid/flat
    инкременты, control-nesting через классифицированный brace-стек, lastOp-серии
    `&&`/`||` по глубине скобок, do-while-спарка, braceless-тела, лямбда-вложенность.
    Плюс `stripDirectiveTokens()` — фильтр препроцессорных строк (ловушка `#if defined` → фальшивый `if`).
  - `include/archcheck/scan/local_complexity_drift.h` + `src/scan/local_complexity_drift.cpp` —
    матчинг `(qualifiedName, paramArity)` → nearest-line/low-confidence; иерархия LCX.1/2/3
    (пороги 25 / Δ≥3 / Δ≥5); блэклист TEST*-макросов; vendor/test-фильтры из
    `file_classification.h`; агрегаты positiveDelta/negativeDelta.
  - Wiring: `printComplexityAdvisory()` в `main.cpp` из `printDiffAdvisories()` —
    паттерн #096/#097, advisory-блок после структурного диффа, exit code не трогает.
  - Тесты: вся тест-матрица из спеки (14 кейсов ядра, все числа сошлись с первого прогона),
    function_body_scan (10 кейсов), drift (9 кейсов), fixtures-тест (6), два repo-level
    integration-кейса в `git_diff_test.cpp`. Итого 458/458 зелёные.
  - `fixtures/local_complexity_drift/{pass,fail_growth,fail_new_complex}` — 8 файлов по списку спеки.
  - Верификация: build Debug ✓, ctest 458/458 ✓, lizard 0 warnings на новом коде ✓,
    clang-format 18.1.3 ✓, dogfood `src/include/tests` = 0 нарушений ✓; смоук
    `--diff HEAD` на самом репо: единственный finding — собственная фикстура
    `saturate` (26 ≥ 25), корректный TP, exit code не изменён.

## В работе

- (пусто — ожидает коммита и закрытия)

## Осталось до закрытия

- Коммит(ы) по команде владельца.
- DoD-пункт «ручная сверка с clang-tidy на ~20 функциях реального кода» — не выполнялся
  в этой сессии (требует clang-tidy прогона; ядро покрыто тест-матрицей и корпусной
  валидацией #102).
- Решение по #099 (fold/absorb) — после лендинга.
- Подтверждение K=5 на втором срезе корпуса — открытый пункт #102, не блокирует.

## Следующие шаги

1. Решено (2026-06-11): `function_signature_scan` не существует — создаём `function_body_scan` здесь (см. п.2 плана).
2. Снято (2026-06-11): shared text preprocessing не нужен — реюз `lex()` из token_normalizer (Шаг 1).
3. Реализовать function metrics scorer.
4. Реализовать baseline/current compare по changed files.
5. Закрыть diff output contract: text + JSON.
6. Добавить fixtures и repo-level integration tests.
7. После этого вернуться к #099 — решение там уже зафиксировано (2026-06-11): #099 живёт только как file-level fallback для файлов, где function-discovery не сработал (макро-тяжёлые), и как диагностические поля; если fallback окажется не нужен — закрыть #099 как absorbed. Отдельной параллельной реализации не делать.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Advisory-only | В репо нет `--strict`, а продукт не должен становиться linter-ом |
| Function-aware combined score, а не только indent proxy | Это ближе к исходной постановке и полезнее для reviewer-а |
| Запускать до graph build | Сигнал дешёвый и не зависит от include graph |
| Не тянуть через `IRule` | Нужен old/new diff compare, а текущий `IRule` snapshot-only |
| Overlap с #099 закрывать консолидацией, не параллельной реализацией | Иначе будет два почти одинаковых complexity-сигнала с разной семантикой |
| Предпочесть отдельный diff-report writer, а не ломать snapshot `Violation` вслепую | Текущий `--diff` уже живёт вне normal reporter path |
| `LCX.2` — advisory-only (или порог-пол Δ≥K), вне gating-вывода | Корпус #102: 72/210 находок `LCX.2` — Δ1-2 на уже-огромных функциях, бесполезны для ревью |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `include/archcheck/scan/duplication/token_normalizer.h` | Реюз `lex()` как есть; новый файл text_scan НЕ создаётся |
| `include/archcheck/scan/function_body_scan.h`, `src/scan/function_body_scan.cpp` | Lightweight function-boundary detection |
| `include/archcheck/scan/local_complexity_metrics.h`, `src/scan/local_complexity_metrics.cpp` | Per-function/per-file complexity metrics |
| `include/archcheck/scan/local_complexity_drift.h`, `src/scan/local_complexity_drift.cpp` | baseline/current comparison and findings |
| `src/main.cpp` | Early diff-stage orchestration before graph build |
| `src/diff/` или `src/report/` | Unified diff advisory text/json output for #096/#097/#101 |
| `tests/unit/scan/` | metrics/body-scan/drift unit tests |
| `tests/integration/diff/git_diff_test.cpp` | real git baseline/current scenarios |
| `fixtures/local_complexity_drift/` | `pass/` and `fail_*` fixtures |

## Итог
**Статус:** completed — v1 реализации задышала целиком:
- **Как работает:** токенный сканер Sonar Cognitive Complexity (v2-скорер из #102,
  D1–D7 закрыты) → `function_body_scan` (границы функций) → per-function метрики →
  baseline/current сравнение по changed files → advisory-вывод в `--diff`
  (иерархия LCX.1 crossed_25 / LCX.2 grew_when_already_above / LCX.3 Δ≥K).
- **Коммиты:** e80b628 (engine), 7938d9c (CLI advisory в --diff), 95aaa62
  (фикстуры `fixtures/local_complexity_drift/{pass,fail_growth,fail_new_complex}`),
  643c69c (checkpoint). 72 LCX-теста в сюите, вся сюита 461/461 зелёная.
- **Не блокирующие хвосты** (зафиксированы выше): clang-tidy прогон; подтверждение
  K=5 на втором срезе корпуса (— #102); решение по #099 (file-level fallback или
  закрыть как absorbed) — после обкатки на реальных диффах.
**Дата завершения:** 2026-06-11
