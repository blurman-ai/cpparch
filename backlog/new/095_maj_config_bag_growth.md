# [CHEAP-DRIFT][SCAN] Config-Bag Growth

**Дата создания:** 2026-06-10
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN / DIFF / REPORT
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #030 (baseline_file_flag), #051 (config_loader_v1), #075 (trusted_diff_workflow), #089 (boolean_state_drift), #090 (bool_field_accretion — образец delta-first advisory), #093 (flag_argument), #094 (param_count_and_accretion)

## Цель

Находить структуры/классы, которые постепенно превращаются в "мешок опций":

- слишком много полей в `*Config` / `*Options` / `*Settings` и соседних типах;
- заметный прирост полей относительно базовой ревизии.

Это cheap heuristic про накопление конфигурации и неявного состояния, а не общий class-metrics tool.

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 3.
- Постановка логически продолжает cheap-drift family из #093/#094: сначала флаги и сигнатуры, потом data carriers, в которые складывают всё подряд.
- В текущем shipped runtime нет production-rule про boolean-field growth; есть только research around boolean-state drift (#089). Значит, здесь речь не о "расширении текущего shipped check", а о новом cheap advisory signal.
- Чтобы не скатиться в generic "измеряем все классы подряд", первый проход должен работать только по candidate names (`*Config`, `*Options`, `*Settings`, `*Params`, `*Context` и т.п.).
- History-growth по `git log` в исходной постановке упоминается как опция, но в текущем продукте полезнее сначала закрыть full scan + diff/ref comparison. Историческое окно лучше выносить только если после v1 останется ясная потребность.

## План выполнения

### Detection contract

- `CFG.1.config_bag_size`:
  - finding, если candidate type перевалил за порог числа полей;
  - full snapshot остаётся advisory.
- `CFG.2.config_bag_accretion`:
  - finding, если candidate type заметно вырос по числу полей относительно base ref;
  - gate-кандидат допустим только для новых полей в changed type и только после фикстурной noise-eval.

### Scanner shape

- Найти `struct|class <Name> { ... };` приближённо, без AST: токен `struct`/`class` +
  идентификатор + (опц. `final`, `: базы` до `{`) + matching brace. Вложенные типы —
  отдельные `TypeInfo` (имя `Outer::Inner`). Forward-декларации (`;` вместо `{`) — скип.
- Считать вероятные поля (по токенам внутри тела, глубина вложенных `{}` = 0):
  - statement до `;`, в котором нет `(` до `;` (отсекает методы/конструкторы),
    нет `using`/`typedef`/`friend`/`static_assert`/`template`;
  - `static` поля не считать (не состояние инстанса);
  - несколько деклараторов через запятую (`bool a, b;`) = число запятых + 1;
  - bitfields (`int x : 3;`) — обычное поле;
  - access labels `public:`/`private:`/`protected:` — скип-токены.
- Кандидат-имена: суффиксы `Config|Options|Settings|Params|Parameters|Opts`
  (case-insensitive по последнему слову имени). `*Context`/`*State` — вне дефолта.
- Пороги v1: `CFG.1` — `fieldCount > 15` (Hadoop-кейс Xu FSE 2015 — рост 17→173;
  меньше 15 у легитимных конфигов сплошь и рядом); `CFG.2` — `+3` полей за один
  diff ИЛИ `+5` относительно base ref без единого удаления. Калибровать на корпусе
  (наш eye-check: config-bag ≈ 40% boolean-кандидатов — порог должен резать FP).
- Матчить old/new по `file + qualified type name` (rename типа = new, без finding).

### Scope discipline

- По умолчанию не анализировать все типы подряд.
- `*State` не включать в дефолтный candidate list, пока не появится подтверждение, что это не ломает точность.
- Если поле объявлено на нескольких строках или несколько полей идут в одной декларации, допускается приблизительный подсчёт, но поведение должно быть зафиксировано тестами.
- Отдельный config surface для suffix list и thresholds — не в этой задаче, если он требует расширения schema.

### Конкретный план в текущем коде

1. Реализовать отдельный lightweight type scanner `include/archcheck/scan/type_body_scan.h` + `src/scan/type_body_scan.cpp`, а не пытаться натянуть это на `function_signature_scan`.
2. Внутри него переиспользовать shared `text_scan.cpp` из #093:
   нужен уже очищенный текст с сохранёнными строками, иначе access labels, multiline declarations и comments быстро ломают счётчик полей.
3. Возвращать `TypeInfo { name, kind, startLine, endLine, fieldCount, isCandidateName }`.
   Candidate suffix list держать как `constexpr std::array` внутри scanner-а, не в текущем config loader.
4. Full scan делать через `collectNonVendoredSources()` и `DiskFileSource`, чтобы сразу унаследовать те же exclusions, что уже применяются в graph/duplication.
5. `CFG.2.config_bag_accretion` считать только по changed files:
   список путей из `git::changedCppFiles()`;
   old/new contents через `GitObjectFileSource` и `DiskFileSource`;
   match по `file + type name`.
6. Не трогать [src/config/config_loader.cpp](~/projects/cpparch/src/config/config_loader.cpp):
   текущая схема знает только `version/modules/rules/thresholds`, и эта задача не должна расширять schema ради suffix list.
7. Тесты разложить так:
   новый `tests/unit/scan/type_body_scan_test.cpp` для candidate names, field counting и access labels;
   при необходимости 1-2 end-to-end compare сценария в [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/config_bag_growth/pass/non_candidate_struct.cpp`
- `fixtures/config_bag_growth/pass/small_options.cpp`
- `fixtures/config_bag_growth/fail_size/render_options.cpp`
- `fixtures/config_bag_growth/fail_accretion/network_config_baseline.cpp`
- `fixtures/config_bag_growth/fail_accretion/network_config_current.cpp`
- `fixtures/config_bag_growth/pass/methods_not_counted.cpp`
- `fixtures/config_bag_growth/pass/access_labels.cpp`

Обязательные проверки:

- candidate-name filter работает;
- non-candidate типы по умолчанию игнорируются;
- методы не считаются полями;
- access labels не ломают счёт;
- рост полей между old/new детектится;
- сигнал остаётся advisory-first и не наказывает legacy full tree.

## Критерии готовности

- Без AST / libclang.
- Без нового top-level config block.
- Исторический `git log` режим не обязателен для v1.
- Зафиксирован candidate suffix list для первого прохода.
- Есть fixtures `pass/` и `fail_*`, как требует backlog policy для правил.

## Не делать

- Универсальный "class size" detector по любым именам.
- Семантическую классификацию "какое поле действительно config/state".
- Git-history growth внутри этой же задачи, если это мешает закрыть diff/full core.
- Отдельную ML/score систему "насколько тип плохой".

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Зафиксировать candidate suffix list для первого прохода.
2. Реализовать scanner диапазона `class/struct`.
3. Реализовать счётчик полей и old/new comparison.
4. Накрыть fixtures на access labels, methods и multiline cases.
5. Проверить signal на dogfood и 1-2 внешних OSS проектах.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Анализировать только `*Config`-подобные типы | Иначе правило быстро становится шумной общей метрикой |
| History-growth вынести из core v1 | Сначала нужен полезный и дешёвый full/diff path |
| `*State` не включать по умолчанию | Слишком высокий риск ложных срабатываний на легитимных state-carrier типах |
| Field counting допускается приближённый, но тестируемый | Цель — дешёвый drift signal, а не точный parser |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/scan/` или `src/cheap_drift/` | Scanner классов/структов и счётчик полей |
| `src/main.cpp` / orchestration | Full scan и comparison against base ref |
| `tests/unit/` | Подсчёт полей и candidate-name filtering |
| `tests/integration/` | Diff/accretion сценарии |
| `fixtures/config_bag_growth/` | `pass/` и `fail_*` фикстуры |
