# [CORE][DOCS][BACKLOG] Свести техдолг по контрактам, CLI-оркестрации и рассинхрону документов

**Дата создания:** 2026-06-02
**Дата старта:** —
**Статус:** new
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

- [ ] Решение по `--config`: validate-only vs real runtime semantics.
- [ ] Свести diagnostic contract (`line`/`column`) к реальности.
- [ ] Свести пороги `GodHeader` между `check` и `--diff`.
- [ ] Привязать config discovery к `root`, не к `cwd`.

### P1 — стабилизировать application layer

- [ ] Разрезать `src/main.cpp`.
- [ ] Вытащить baseline/diff/config helpers в отдельные application units.
- [ ] Дожать SF.8 до корректной guard-semantics.

### P2 — синхронизировать документы и backlog

- [ ] Закрыть #045.
- [ ] Обновить `AGENTS.md`.
- [ ] Нормализовать duplication roadmap и убрать stale `#053` narrative.
- [ ] Почистить `wip`/`new`, где ссылки ведут в удалённые артефакты.

### P3 — hygiene research vs product

- [ ] Зафиксировать policy для `experiments/`.
- [ ] Ограничить использование experimental artifacts как источника истины для shipped behavior.

## Критерий приёмки

- [ ] Пользовательский контракт CLI больше не обещает поведение, которого нет.
- [ ] `check` и `--diff` используют согласованные thresholds/policy.
- [ ] Для config discovery нет зависимости от случайного `cwd`.
- [ ] `docs/MVP.md`, `docs/ROADMAP.md`, `docs/architecture-spec.md`, `AGENTS.md` не противоречат shipped v0.1/v0.2 scope.
- [ ] В `backlog/wip/` не остаётся задач, чьё текущее состояние противоречит архитектурным решениям или ссылается на удалённые деревья.
- [ ] `src/main.cpp` перестаёт быть местом концентрации всей application policy.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Делать одну umbrella-задачу, не россыпь микро-тикетов | техдолг здесь системный и связан причинно; по отдельности он теряет контекст |
| Сначала alignment, потом новые правила | пока базовые контракты недостоверны, каждая новая фича повышает стоимость поддержки |
| Не смешивать docs cleanup с product cleanup | часть пунктов требует решения по поведению, а не переписывания текста |
| Duplication stale-state считать техдолгом, а не «просто research» | backlog/current docs уже потребляют этот narrative как будто он product-relevant |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `backlog/new/073_maj_tech_debt_alignment_cleanup.md` | новая umbrella-задача с инвентаризацией техдолга |

## Следующие шаги

1. Сначала принять решение по `--config` и diagnostic contract.
2. Затем убрать скрытый policy drift (`GodHeader` thresholds, config discovery root/cwd).
3. После этого резать `main.cpp` и закрывать docs/backlog synchronization.
