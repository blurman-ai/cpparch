# [CLI][DIFF][REPORT] MVP v1: довести trusted PR/diff workflow до product-grade

**Дата создания:** 2026-06-02
**Дата старта:** —
**Статус:** new
**Модуль:** CLI / DIFF / REPORT / DOCS
**Приоритет:** critical
**Сложность:** M
**Блокирует:** MVP v1 release readiness
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #023 (diff_skip_when_no_cpp_changes), #024 (in_memory_fs_for_diff), #025 (pr_comment_integration), #028 (rules_engine_mvp), #030 (baseline_file_flag), #073 (tech_debt_alignment_cleanup), #045 (docs_sync_roadmap_mvp_spec)

## Цель

Собрать уже существующие куски `--diff` / baseline / report / CI в **один
канонический пользовательский workflow**, которому можно доверять как первому
продуктовому wedge `archcheck`.

Задача не про новые графовые метрики, а про **productization**:

- один понятный сценарий запуска в CI;
- один понятный отчёт;
- одно понятное поведение exit codes;
- явная граница между advisory и gated режимом.

## Контекст

Фундамент уже шипится:

- `#018` дал `--diff`;
- `#023` и `#024` сделали diff-путь быстрым;
- `#025` показал путь в PR/CI;
- `#030` дал baseline;
- rules/report core уже живёт в продукте.

Но остаток всё ещё размазан по нескольким задачам и файлам. Сейчас есть
техническая реализация diff-анализа, но нет одной чёткой product-story уровня:

> “Вставь одну команду в CI и узнай, внёс ли этот PR новую плохую зависимость.”

Пока этого нет, MVP выглядит как набор фич, а не как собранный продуктовый путь.

## Что должно получиться

Для пользователя должен существовать **один канонический workflow**:

- запустил diff против base;
- увидел, какие новые зависимости/циклы появились;
- понял, advisory это или gated regression;
- получил стабильный text/json output;
- смог скопировать готовый CI пример без чтения half-implemented surfaces.

## Что сделать

### 1. Зафиксировать канонический diff surface

- [ ] Выбрать и задокументировать одну основную форму запуска PR/diff анализа.
- [ ] Явно описать, как эта форма соотносится с baseline-flow и локальным запуском.
- [ ] Убрать из docs/help двусмысленность между “несколько похожих режимов” и
      “один основной продуктовый путь”.

### 2. Зафиксировать product contract отчёта

- [ ] Определить обязательный состав diff-отчёта для MVP v1:
      added edges / new cycles / SCC growth / краткий summary.
- [ ] Сделать `text` и `json` стабильными и детерминированными.
- [ ] Убрать из default output всё, чему пока нельзя доверять как trusted gate.

### 3. Явно развести advisory и gate

- [ ] Зафиксировать advisory-first default.
- [ ] Явно описать, какие regressions считаются gated в MVP v1.
- [ ] Привязать это к exit code поведению, не ломая контракт `0 / 1 / 2 / 3`.

### 4. Собрать E2E покрытие workflow

- [ ] Добавить/дополнить golden или fixture-based сценарии уровня продукта:
      clean diff, new edge, new cycle, harmless change.
- [ ] Проверять именно конечный пользовательский output, а не только внутренние DTO.

### 5. Дать минимальный CI onboarding

- [ ] Один copy-paste пример для PR/CI сценария.
- [ ] Один пример для строгого gate режима, если он поддержан для MVP v1.
- [ ] Убедиться, что roadmap/docs/README ссылаются на один и тот же путь.

## Критерий приёмки

- [ ] У `archcheck` есть один канонический PR/diff workflow, описанный одинаково в code/docs.
- [ ] Default behaviour — advisory-first, без скрытых gating surprises.
- [ ] `text/json` output стабилен и проверяется e2e-фикстурами.
- [ ] Есть copy-paste CI пример без опоры на research/docs archaeology.
- [ ] Пользовательский ответ на вопрос “что делает этот PR с dependency graph?” получается одной командой.

## Не делать в этой задаче

- не тащить duplication в release gate;
- не расширять продукт в сторону AI-attribution;
- не делать runtime config policy (`layers` / `forbidden` / `independence`) — это уже `v0.2`;
- не смешивать с semantic backend / libclang expansion.

## Следующие шаги

1. Сначала закрыть `#073` P0-срез по контрактам.
2. После этого собрать и зафиксировать канонический diff contract.
3. Потом уже добирать richer metrics и optional integrations.
