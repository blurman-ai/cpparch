# [PROCESS] Принять OSS-стандарты для git-процесса

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Статус:** wip
**Модуль:** PROCESS
**Приоритет:** critical
**Сложность:** medium
**Блокирует:** все будущие задачи (раньше зафиксируешь конвенции — меньше дрейфа)
**Заблокирован:** —
**Related:** #006 (spec_refactor — stability contract попадёт в спеку)

## Цель

Зафиксировать общепринятые OSS-стандарты для git-процесса cpparch и применить их к существующей оснастке: GitHub Flow + Conventional Commits + SemVer 2.0 + Keep a Changelog + аннотированные `vX.Y.Z` теги. Дальше никто не сможет придраться к «нестандартному» процессу.

## Контекст

Решение принято в сессии 2026-05-26 после обсуждения. Краткий обзор:

- **GitHub Flow** (не Git Flow). Master всегда зелёный, всё через feature-ветки + PR. Используют fmt, spdlog, Catch2, nlohmann/json, ArchUnit. Sam Driessen (автор Git Flow) в 2020 признал, что для web/OSS его модель overkill.
- **Conventional Commits** ([conventionalcommits.org](https://www.conventionalcommits.org/)). `<type>(<scope>): <description>`. Открывает автоматизацию: commitlint, release-please, auto-changelog.
- **SemVer 2.0** ([semver.org](https://semver.org/)). Pre-1.0 → `0.x.y`, MINOR может ломать. 1.0 — когда CLI/JSON/config-формат стабильны.
- **Keep a Changelog** ([keepachangelog.com](https://keepachangelog.com/)). `CHANGELOG.md` с секциями Added / Changed / Deprecated / Removed / Fixed / Security + Unreleased сверху.
- **Annotated `vX.Y.Z` tags**. Linux, Rust, Go, fmt. Tag — annotated, не lightweight.

Default branch остаётся `master` (переименование в `main` — churn без выгоды).

Ветки именуются `<type>/<NNN>-<short-slug>`, где type ∈ `feat/fix/docs/refactor/chore/test/build`, NNN — ID задачи из backlog. Пример: `docs/006-spec-refactor`, `chore/007-workflow-setup`, `feat/004-project-skeleton`.

## План выполнения

- [x] **1. Написать `docs/dev/git_workflow.md`** (≈30 строк). Содержит:
   - Branching: GitHub Flow + naming convention.
   - Commits: Conventional Commits + словарь scope-ов под cpparch (`config`, `graph`, `scan`, `rules/sf`, `rules/lakos`, `rules/martin`, `report`, `cli`, `fixtures`, `build`, `docs`, `tasks`, `process`).
   - Versioning: SemVer 2.0 + правило pre-1.0.
   - Tags: `vX.Y.Z` аннотированные, push с `--follow-tags`.
   - Changelog: ссылка на Keep a Changelog.
   - Trailers (`AI-Assisted`, `Verified`, `Risk`, `Co-Authored-By`) — оставлены поверх Conventional Commits, парсерами игнорируются.

- [ ] **2. Обновить `/commit` скил** под Conventional Commits.
   - Заменить `<type>: [TAG] subject` → `<type>(<scope>): subject`.
   - Mapping старых тегов в scope-ы: `[CONFIG]→config`, `[GRAPH]→graph`, `[SCAN]→scan`, `[RULES][SF]→rules/sf`, `[RULES][LAKOS]→rules/lakos`, `[RULES][MARTIN]→rules/martin`, `[RULES][CUSTOM]→rules/custom`, `[REPORT]→report`, `[CLI]→cli`, `[FIXTURES]→fixtures`, `[BUILD]→build`, `[DOCS]→docs`, `[DOCS][CLAUDE]→docs/claude`, `[DOCS][TASKS]→tasks`, `[PROCESS]→process`.
   - Subject ≤72 символов на первой строке. Body разделён пустой строкой.

- [ ] **3. Обновить `/create-task` скил.**
   - После создания файла предлагать команду создания ветки `<type>/<NNN>-<slug>` (тип угадывать по модулю; пользователь подтверждает).

- [x] **4. Завести `CHANGELOG.md` по Keep a Changelog.**
   - Шапка с ссылкой на keepachangelog.com и semver.org.
   - Секция `[Unreleased]` с пустыми подсекциями Added/Changed/Deprecated/Removed/Fixed/Security.
   - Готов под первый релиз `0.1.0`.

- [x] **5. Дописать stability contract** в `docs/architecture-spec.md`.
   - Что считается breaking change (MAJOR bump): изменение exit codes, удаление CLI-флага, изменение схемы JSON-отчёта, изменение схемы YAML-конфига, изменение формата baseline.
   - Что считается non-breaking (MINOR): добавление CLI-флагов с default, добавление полей в JSON-отчёт, новые дефолтные правила (с опцией выключения).
   - Pre-1.0 — semver разрешает breaking в MINOR, но проект всё равно стремится их избегать без явной нужды.

- [x] **6. Короткие правки в CLAUDE.md и backlog/README.md.**
   - Упомянуть `docs/dev/git_workflow.md` как канон.
   - В backlog/README.md секция «Жизненный цикл задачи» — добавить шаг про создание ветки.

- [x] **7. (опционально) Уведомить пользователя про self-approve.**
   - В git_workflow.md — заметка: для solo-dev режима поставить `Required approvals = 0` в GitHub Rulesets для master.

## Сделано

- **2026-05-26** — Шаги 1, 4, 5, 6, 7: завёл [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md) (полный канон: GitHub Flow + Conventional Commits + SemVer + KAC + теги + release-процесс), [`CHANGELOG.md`](../../CHANGELOG.md) (Keep a Changelog 1.1, `[Unreleased]` накопительная), секция «Stability contract» в [`docs/architecture-spec.md`](../../docs/architecture-spec.md) (таблица breaking vs non-breaking по exit codes / CLI / JSON / SARIF / YAML-config / baseline / дефолтные правила). Ссылка на workflow добавлена в `CLAUDE.md` и шаг 2 жизненного цикла в `backlog/README.md`. Self-approve / direct push для admin зафиксированы в workflow doc.

## В работе

- (пусто)

## Следующие шаги

1. Открыть `docs/dev/git_workflow.md` — написать с нуля.
2. Параллельно подготовить mapping старых тегов в scope-ы (см. п. 2 плана).
3. После — обновить `/commit` и `/create-task`, перепроверить, что они согласованы между собой.
4. Создать `CHANGELOG.md` — последний шаг.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| GitHub Flow, не Git Flow | Git Flow устарел даже по мнению автора; для CLI / OSS overkill |
| Default branch остаётся `master` | Переименование в `main` — косметика, не стоит churn-а |
| Conventional Commits, не custom | Открывает commitlint, release-please, parsable history. Стандарт с известными tooling-партнёрами |
| SemVer pre-1.0 = `0.x.y` | Стандарт SemVer 2.0 §4; даёт легитимность ломки в MINOR до v1.0 |
| `v`-префикс в тегах | Чаще встречается в OSS (Rust, Go, fmt), Linux kernel — исключение |
| Keep a Changelog, не auto-generated | Курируемый changelog читается людьми; auto-gen всегда можно добавить позже |
| Trailers сохранить | Игнорируются Conventional-Commits парсерами, ценны как AI-аудит-маркеры |
| PROCESS как новый scope/tag | Существующие BUILD/DOCS не подходят семантически |

## Изменённые файлы

| Файл | Изменение | Commit |
|------|-----------|--------|
| `docs/dev/git_workflow.md` | новый — канон конвенций | pending |
| `.claude/commands/commit.md` | conventional commits + scope mapping | pending (шаг 2) |
| `.claude/commands/create-task.md` | предложение создать ветку после задачи | pending (шаг 3) |
| `CHANGELOG.md` | новый — Keep a Changelog шаблон с `[Unreleased]` | pending |
| `docs/architecture-spec.md` | секция «Stability contract» добавлена | pending |
| `CLAUDE.md` | ссылка на git_workflow.md в Code style & AI constraints | pending |
| `backlog/README.md` | feature-ветка как опция в шаге 2 «Старт работы» | pending |
