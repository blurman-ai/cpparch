# [CORE][DOCS][BACKLOG] Свести техдолг по контрактам, CLI-оркестрации и рассинхрону документов

**Дата создания:** 2026-06-02
**Дата старта:** 2026-06-11
**Дата завершения:** 2026-06-12
**Статус:** completed
**Модуль:** CORE / CLI / DOCS / BACKLOG
**Приоритет:** major
**Сложность:** L
**Блокирует:** —
**Заблокирован:** —
**Related:** #045 (docs_sync_roadmap_mvp_spec), #051 (config_loader_v1), #057 (lakos_fanout_coupling_checks), #060 (checker_validation_hardening_loop), #070 (checker_fp_fix_proposals)
**Источник истины:** [docs/architecture-spec.md](../../docs/architecture-spec.md), [docs/ROADMAP.md](../../docs/ROADMAP.md), [docs/config_format.md](../../docs/config_format.md)

## Цель

Собрать в одну задачу накопившийся **неслучайный техдолг** вокруг shipped v0.1:
не новые фичи, а расхождения между кодом, CLI-контрактами, документами и backlog.
Задача нужна как umbrella: чтобы сначала выровнять продуктовую реальность, а уже
потом безопасно расширять правила и исследовательские ветки.

## Контекст

Кодовое ядро `archcheck` уже живое: есть `scan -> graph -> rules -> report`,
diff/drift, baseline, config loader, unit/integration tests. Но вокруг ядра
накопился слой долгов двух типов:

1. **Продуктовые контракты расходятся с реальной реализацией.**
2. **Документы и backlog расходятся между собой и с текущим деревом.**

Это уже не «косметика в доках». Часть расхождений напрямую вводит пользователя в
заблуждение (`--config`, формат diagnostics, разные пороги в `check` и `--diff`).

## TL;DR

Главный долг не в графовом ядре, а в **alignment**:

- CLI обещает больше, чем исполняет.
- Модель данных уже не покрывает обещанный output contract.
- `main.cpp` перерос в application-god-file и стал источником расхождения режимов.
- roadmap/backlog/research документы описывают разные состояния продукта.
- Часть `wip`-задач ссылается на уже удалённые артефакты.

Пока это не сведено, любая новая фича повышает стоимость сопровождения непропорционально.

## Инвентаризация техдолга

### 1. Ложный контракт `--config`

- CLI help обещает `--config <path> ... run check`.
- Loader реально парсит `modules` + `layers` / `independence` / `forbidden`.
- Но runtime использует из `Config` только `thresholds`; модульные правила не
  применяются вообще.

Следствие:

- пользователь получает «конфиг принят», но архитектурные правила из конфига не работают;
- у нас есть shipped surface area без shipped semantics;
- это опаснее обычного TODO, потому что поведение выглядит завершённым.

## Что сделать

- [ ] Принять продуктовое решение: либо `--config` временно validate-only, либо
      довести config-rules до runtime.
- [ ] Если validate-only: честно поменять help/README/spec и exit behavior.
- [ ] Если runtime: отдельная задача на `config -> rule pipeline`, без маскировки под «мелкий фикс».

### 2. Диагностический контракт расходится с моделью `Violation`

- Документы и AGENTS-фрейминг обещают `file:line:column`.
- `Violation` хранит только `file`, `line`, `message`, `ruleId`.
- Text/JSON reporters колонку не выводят, потому что её просто негде взять.

Следствие:

- пользовательский контракт уже шире, чем реальная доменная модель;
- дальше будет больно добавлять точные location-based rules и suppression.

## Что сделать

- [ ] Принять один контракт: либо реально расширить `Violation` до `column`,
      либо понизить все документы до `file[:line]`.
- [ ] Если добавляем `column`: обновить baseline/json schema и сразу зафиксировать migration policy.

### 3. Разные пороги у `check` и `--diff`

- Обычный `Lakos.GodHeader` живёт на пороге `50`.
- Diff/report слой держит свой дефолт `30`.
- Это разные источники истины для одной и той же сущности.

Следствие:

- один и тот же проект может быть зелёным в `check` и красным в `--diff`;
- метрики и regression semantics становятся недоверяемыми.

## Что сделать

- [ ] Свести threshold defaults к одному месту.
- [ ] Пробросить их в diff-mode явно, а не через скрытые литералы.
- [ ] Покрыть тестом именно консистентность между `check` и `--diff`.

### 4. `src/main.cpp` стал application god-file

- Сейчас там ~570 строк оркестрации: parsing CLI, baseline I/O, graph preview,
  diff-mode, drift-mode, config discovery, report dispatch.
- Именно здесь уже живут режимные расхождения и дублированные правила выбора defaults.

Следствие:

- новые режимы добавлять всё дороже;
- легко получить ещё одно поведение «только этот режим забыл пробросить X»;
- application layer перестал быть очевидным.

## Что сделать

- [ ] Разрезать CLI-оркестрацию на маленькие application-level команды:
      `check`, `diff`, `graph`, `scan`, baseline helpers.
- [ ] Убрать из `main.cpp` policy decisions, которые должны жить в shared layer.
- [ ] Оставить в `main.cpp` только thin dispatch.

### 5. SF.8 проверяется слабее, чем заявлено

- Текущая эвристика считает header guarded, если в первых строках увидела любой `#ifndef`.
- Проверки пары `#ifndef` + `#define` одного и того же guard-а нет.
- Это создаёт ложные отрицания и делает правило менее строгим, чем выглядит по названию.

## Что сделать

- [ ] Усилить SF.8 до реального include-guard pattern.
- [ ] Не размазывать логику: reuse уже существующего scanner knowledge, где возможно.
- [ ] Добавить fixtures на ложный `#ifndef` без guard semantics.

### 6. Discovery config зависит от `cwd`, а не от проверяемого root

- `archcheck /path/to/project` ищет `.archcheck.yml` от текущей директории процесса.
- Это делает поведение неочевидным для CI, monorepo и внешнего запуска.

## Что сделать

- [ ] Привязать discovery к `root`, который реально проверяется.
- [ ] Явно задокументировать precedence: `--config` > discovered-near-root > embedded defaults.

### 7. Документы описывают несколько разных продуктов

- `docs/MVP.md` всё ещё про старый pre-#006 MVP с `compile_commands.json` и dependency rules как ядром.
- `docs/ROADMAP.md` одновременно тащит duplication line-pass `#053` в текущий scope.
- `docs/architecture-spec.md` в ряде мест ещё использует устаревший vocabulary
  (`forbidden_deps` / `allowed_deps`) рядом с уже принятым typed config contract.
- `AGENTS.md` критически устарел: пишет `pre-implementation`, «нет src/tests/CMake», старые naming rules.

Следствие:

- агент или новый разработчик не может быстро понять, что реально shipped;
- при конфликте документов у нас нет стабильной опоры;
- исследовательские решения маскируются под product reality.

## Что сделать

- [ ] Закрыть #045 как ближайший docs-blocker.
- [ ] Обновить `AGENTS.md` до фактического состояния репо и текущего style contract.
- [ ] Убрать из публичных документов формулировки, которые обещают v0.2-фичи как v0.1-core.

### 8. Backlog содержит stale WIP и битые ссылки на удалённые артефакты

- `docs/duplication_architecture.md` фиксирует, что line-based `#053` и его дерево удалены.
- При этом `backlog/wip/053_...` всё ещё активен и ссылается на отсутствующие `experiments/line_duplication/*`.
- Roadmap продолжает считать `#053` частью активного v0.1 narrative.

Следствие:

- backlog теряет ценность как система состояния;
- старые исследования выглядят как «в работе», хотя уже отменены архитектурным решением.

## Что сделать

- [ ] Перевести отменённые ветки из `wip` в корректное состояние: закрыть, архивировать или переписать как historical note.
- [ ] Почистить ссылки на удалённые артефакты.
- [ ] Свести duplication narrative к одному живому пути: `#056` + `#070` + `#060`.

### 9. Исследовательский слой разросся сильнее продуктового

- В `src/include/tests` меньше сотни файлов.
- В `experiments/` уже сильно больше тысячи файлов.
- Это нормально для research-фазы, но уже требует отдельной hygiene policy.

Следствие:

- signal-to-noise при навигации по репо падает;
- backlog и docs начинают ссылаться на экспериментальные артефакты как на product truth;
- stale research artifacts дольше живут без решения.

## Что сделать

- [ ] Зафиксировать правило: что считается product source of truth, а что experiment-only.
- [ ] Ограничить прямые ссылки из roadmap/current docs на экспериментальные файлы без ADR/summary слоя.
- [ ] При необходимости вынести крупные исследования в подкаталоги с явным lifecycle-статусом.

### 10. Мелкий, но системный долг по single source of truth

- literal thresholds и policy живут в нескольких местах;
- docs/code_style и AGENTS расходятся по naming (`name_` vs `_name`, `I*` vs no-`I`);
- часть completed/new задач уже фиксирует одно решение, а спецификация ещё другое.

Следствие:

- каждый следующий change-set требует синхронизации вручную;
- вероятность тихого повторного рассинхрона высокая.

## Что сделать

- [ ] Для каждого shipped user-facing контракта иметь ровно один product source of truth.
- [ ] Явно разделить: spec / roadmap / historical backlog notes / experiment reports.

## План выполнения

### P0 — убрать недоверяемые контракты

- [x] Решение по `--config`: validate-only vs real runtime semantics. *(закрыто #082 Slice 1/2: validate-only)*
- [x] Свести diagnostic contract (`line`/`column`) к реальности. *(закрыто #082: понижено до `file:line` везде)*
- [x] Свести пороги `GodHeader` между `check` и `--diff`. *(2026-06-11: diff берёт порог из discovered config; дефолт `MetricThresholds` 30 → 50; тест-пин на равенство дефолтов)*
- [x] Привязать config discovery к `root`, не к `cwd`. *(2026-06-11: `discoverConfig(root)`, walkup от анализируемого root; precedence задокументирован в config_format.md)*

### P1 — стабилизировать application layer

- [x] Разрезать `src/main.cpp`. *(2026-06-12: 786 → 275 строк; thin dispatch + `src/cli/` units)*
- [x] Вытащить baseline/diff/config helpers в отдельные application units. *(2026-06-12: check/baseline/drift → `cli/check_command`, diff → `cli/diff_command`, preview → `cli/preview_commands`, discovery → `config::discover`)*
- [x] Дожать SF.8 до корректной guard-semantics. *(2026-06-11: требуется пара `#ifndef NAME` + `#define NAME`; lone `#ifndef` (NDEBUG-tweak) больше не считается guard; фикстура `fail_ifndef_no_define/` + 3 unit-теста)*

### P2 — синхронизировать документы и backlog

- [x] Закрыть #045. *(закрыто, commit 87878fa)*
- [x] Обновить `AGENTS.md`. *(закрыто #082 Slice 3: полный rewrite)*
- [x] Нормализовать duplication roadmap и убрать stale `#053` narrative. *(закрыто: #053 → `backlog/dropped/`, #085 completed, duplication = shipped advisory per #082 Slice 5b)*
- [x] Почистить `wip`/`new`, где ссылки ведут в удалённые артефакты. *(закрыто #082 Slice 4: 26 битых ссылок исправлено)*

### P3 — hygiene research vs product

- [x] Зафиксировать policy для `experiments/`. *(закрыто: `experiments/` в `.gitignore` как локальная песочница, вне git)*
- [x] Ограничить использование experimental artifacts как источника истины для shipped behavior. *(закрыто #082: текущие docs/AGENTS/roadmap не ссылаются на `experiments/`)*

## Сделано

### Ребейз инвентаризации (2026-06-11)

Задача написана 2026-06-02; umbrella **#082** (2026-06-05, completed) закрыла
бо́льшую часть пунктов: `--config` → validate-only (решение зафиксировано в #082),
diagnostics → `file:line`, AGENTS.md rewrite, link hygiene, duplication-статус,
плюс #045 (commit 87878fa) и backlog-pass'ы (#053 → dropped/, #085). Из 10 пунктов
живыми на 2026-06-11 остались только №3 (пороги diff), №4 (main.cpp), №5 (SF.8), №6 (discovery cwd).

### Пороги `check` vs `--diff` (п.3) — 2026-06-11

- `MetricThresholds::godHeaderFanIn` 30 → 50 (= дефолт `config::Thresholds`), комментарий-якорь в header.
- `--diff` теперь discovery-ит `.archcheck.yml` от repo root и пробрасывает
  `thresholds.godHeaderFanIn` в `buildRegressionReport` (раньше — скрытый литерал 30);
  `ConfigError` в diff-режиме → exit 2 (контракт).
- Тест-пин: `MetricThresholds{} == Config{}.thresholds` (git_diff_test.cpp).

### Config discovery от root (п.6) — 2026-06-11

- `discoverConfig(root)`: walkup от анализируемого root, не от CWD процесса;
  `archcheck /path/to/project` подхватывает конфиг проекта из любого места запуска.
- `docs/config_format.md`: формулировка discovery + явный precedence
  `--config` > discovered-near-root > embedded defaults.

### SF.8 guard-semantics (п.5) — 2026-06-11

- Guard = пара `#ifndef NAME` + последующий `#define NAME` того же макроса в окне
  60 непустых строк (несколько pending `#ifndef` допускается, `#define` другого имени не закрывает guard).
- Lone `#ifndef` (типичный кейс `#ifndef NDEBUG`) больше не проходит за guard — устранён false negative.
- `#pragma once`, BOM, ObjC, `.inc` — поведение не изменилось; dogfood `src/ include/ tests/` — 0 нарушений.

Верификация: build Debug, 465/465 тестов, dogfood 0 нарушений, smoke `--diff` на
реальном revspec, lizard/clang-format чисто.

### Разрез `main.cpp` (п.4) — 2026-06-12

Behavior-preserving рефакторинг четырьмя срезами (каждый ≤2 новых файлов,
сборка+тесты зелёные после каждого):

- **c1** — `config::discover(root)` перенесён в config-подсистему
  (`config_loader.{h,cpp}`): shared discovery policy для check и diff,
  +2 unit-теста (walkup-найденный конфиг / embedded defaults).
- **c2** — `src/cli/check_command.{h,cpp}`: `OutputFormat`/`BaselineMode`/`BaselineOpts`,
  baseline save/load/filter, drift-gate policy, `runCheck`, `runSaveGraphBaseline`.
- **c3** — `src/cli/preview_commands.{h,cpp}`: `runScan`/`runGraph`/`runDuplication`/`runHistory`.
- **c4** — `src/cli/diff_command.{h,cpp}`: `DiffMode`, `runDiff` + advisories
  (SATD, test co-evolution, local complexity drift).

`main.cpp`: 786 → 275 строк — только help/version, argv-парсинг (`dispatch_*`),
`main()`. CLI-юниты — внутренние заголовки `src/cli/*.h` (не в `include/archcheck/` —
не публичный API), executable получил `target_include_directories(PRIVATE src/)`.
Переехавшие функции переименованы в `lowerCamelCase` per code_style (`run_check` →
`cli::runCheck`); тела не менялись. Известное наследие: lizard-warning на
`applyBaselineAndReport` (CCN 12, 38 строк) существовал и в main.cpp — не новый долг.

Верификация: build Debug, 467/467 тестов, dogfood `src/ include/ tests/` 0 нарушений
(новые заголовки проходят SF.7/8/9), smoke всех режимов CLI (check/json/config/
baseline save+load/graph-baseline/drift/scan/graph/diff), clang-format 18.1.3 чисто.

## Критерий приёмки

- [x] Пользовательский контракт CLI больше не обещает поведение, которого нет. *(#082)*
- [x] `check` и `--diff` используют согласованные thresholds/policy. *(2026-06-11)*
- [x] Для config discovery нет зависимости от случайного `cwd`. *(2026-06-11)*
- [x] `docs/MVP.md`, `docs/ROADMAP.md`, `docs/architecture-spec.md`, `AGENTS.md` не противоречат shipped v0.1/v0.2 scope. *(#082 + #045)*
- [x] В `backlog/wip/` не остаётся задач, чьё текущее состояние противоречит архитектурным решениям или ссылается на удалённые деревья. *(#082 Slice 4 + backlog-pass'ы, #053 → dropped/)*
- [x] `src/main.cpp` перестаёт быть местом концентрации всей application policy. *(2026-06-12: thin dispatch, 275 строк)*

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Делать одну umbrella-задачу, не россыпь микро-тикетов | техдолг здесь системный и связан причинно; по отдельности он теряет контекст |
| Не дублировать работу #082 — ребейз инвентаризации перед стартом | #082 (XL umbrella, 2026-06-05) закрыла пп.1,2,7,8,9,10; здесь остаются только пп.3,4,5,6 |
| Дефолт diff-порога выровнен на 50 (= check), а не наоборот | 50 — документированный авторитетный порог Lakos.GodHeader (help, config_format.md); 30 был скрытым литералом diff-слоя |
| Diff discovery-ит конфиг от repo root (worktree-версия `.archcheck.yml`) | состояние worktree — текущая policy проекта; конфиг из baseline-ref не читаем |
| SF.8: пара ifndef+define, без поддержки `#if !defined(...)` | покрывает заявленную guard-semantics без расширения скоупа; `!defined`-форма и раньше не принималась |
| Сначала alignment, потом новые правила | пока базовые контракты недостоверны, каждая новая фича повышает стоимость поддержки |
| Не смешивать docs cleanup с product cleanup | часть пунктов требует решения по поведению, а не переписывания текста |
| Duplication stale-state считать техдолгом, а не «просто research» | backlog/current docs уже потребляют этот narrative как будто он product-relevant |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `backlog/wip/073_maj_tech_debt_alignment_cleanup.md` | umbrella-задача; 2026-06-11 ребейз по #082 + лог прогресса |
| `include/archcheck/diff/regression_report.h` | дефолт `godHeaderFanIn` 30 → 50 + комментарий-якорь на config.h |
| `src/main.cpp` | `discoverConfig(root)`; diff пробрасывает thresholds из конфига, ConfigError → exit 2 |
| `tests/integration/diff/git_diff_test.cpp` | тест-пин равенства дефолтов check/diff |
| `docs/config_format.md` | discovery от analyzed root + явный precedence |
| `src/rules/sf8_include_guard.cpp` | guard = пара `#ifndef`+`#define` одного макроса (`directiveArg`) |
| `tests/unit/rules/sf8_include_guard_test.cpp` | 3 теста: lone ifndef, чужой define, комментарий между парой |
| `fixtures/sf8_include_guard/fail_ifndef_no_define/ndebug_tweak.h` | фикстура false-guard (`#ifndef NDEBUG`) |
| `include/archcheck/config/config_loader.h`, `src/config/config_loader.cpp` | c1: `config::discover(root)` — shared discovery policy |
| `tests/unit/config/test_loader.cpp` | c1: 2 теста на `discover` |
| `src/cli/check_command.{h,cpp}` | c2: check + baseline/drift/report policy |
| `src/cli/preview_commands.{h,cpp}` | c3: scan/graph/duplication/history |
| `src/cli/diff_command.{h,cpp}` | c4: diff-режим + advisories |
| `src/main.cpp` | 786 → 275 строк: help/version + thin argv-dispatch |
| `src/CMakeLists.txt` | cli/*.cpp в executable, PRIVATE include на src/ |

## Как работает

Umbrella закрыта в три слоя. (1) Контрактные фиксы: дефолт `MetricThresholds`
выровнен на 50 и запинен тестом к `config::Thresholds`, `--diff` берёт порог из
discovered-конфига; discovery (`config::discover(root)`) идёт walkup от
анализируемого root, а не CWD процесса. (2) SF.8: guard засчитывается только при
паре `#ifndef NAME` + `#define NAME` (helper `closesGuardPair`, окно 60 непустых
строк) — lone `#ifndef` больше не проходит. (3) Application layer: `main.cpp` —
thin dispatch (help/version + argv-парсинг), вся командная policy — во внутренних
юнитах `src/cli/` (`check_command`, `diff_command`, `preview_commands`).

## Итог

**Статус:** completed
**Дата завершения:** 2026-06-12

Из 10 пунктов инвентаризации 6 закрыла umbrella #082 (+#045, backlog-pass'ы) —
зафиксировано ребейзом; оставшиеся 4 (пороги diff, discovery root, SF.8,
main.cpp) закрыты здесь. Все критерии приёмки выполнены. Верификация: build
Debug, 467/467 тестов, dogfood `src/ include/ tests/` 0 нарушений, smoke всех
CLI-режимов, clang-format 18.1.3 / cppcheck / lizard чисто, coverage
92.8/96.3/59.8 PASS. Известное наследие: lizard-warning `applyBaselineAndReport`
(CCN 12) — переехал из main.cpp как есть, не новый долг.
