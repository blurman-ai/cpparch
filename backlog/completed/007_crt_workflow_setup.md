# [PROCESS] Принять OSS-стандарты для git-процесса

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
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

- [x] **2. Обновить `/commit` скил** под Conventional Commits.
   - Заменить `<type>: [TAG] subject` → `<type>(<scope>): subject`.
   - Mapping старых тегов в scope-ы: `[CONFIG]→config`, `[GRAPH]→graph`, `[SCAN]→scan`, `[RULES][SF]→rules/sf`, `[RULES][LAKOS]→rules/lakos`, `[RULES][MARTIN]→rules/martin`, `[RULES][CUSTOM]→rules/custom`, `[REPORT]→report`, `[CLI]→cli`, `[FIXTURES]→fixtures`, `[BUILD]→build`, `[DOCS]→docs`, `[DOCS][CLAUDE]→docs/claude`, `[DOCS][TASKS]→tasks`, `[PROCESS]→process`.
   - Subject ≤72 символов на первой строке. Body разделён пустой строкой.

- [x] **3. Обновить `/create-task` скил.**
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

- **2026-05-26** — Шаги 1, 4, 5, 6, 7: завёл [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md) (полный канон: GitHub Flow + Conventional Commits + SemVer + KAC + теги + release-процесс), [`CHANGELOG.md`](../../CHANGELOG.md) (Keep a Changelog 1.1, `[Unreleased]` накопительная), секция «Stability contract» в [`docs/architecture-spec.md`](../../docs/architecture-spec.md) (таблица breaking vs non-breaking по exit codes / CLI / JSON / SARIF / YAML-config / baseline / дефолтные правила). Ссылка на workflow добавлена в `CLAUDE.md` и шаг 2 жизненного цикла в `backlog/README.md`. Self-approve / direct push для admin зафиксированы в workflow doc. Коммит `930e323`.
- **2026-05-26** — Post-hoc правка: из секции «Stability contract» убрана завершающая ссылка на `docs/dev/git_workflow.md` — спека про *what*, workflow про *how*, не смешивать слои. Коммит `f19c130`.
- **2026-05-26** — Шаги 2, 3: `/commit` скил переписан под Conventional Commits 1.0 (таблицы type/scope из git_workflow.md, формат `<type>(<scope>): <subject>`, scope-map из старых `[TAG]`, subject ≤72, lowercase, императив). `/create-task` дополнен секцией «Когда начинаешь работу — выбор формата»: direct push для рутины vs feature-ветка `<type>/<NNN>-<slug>` для значимой работы.

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
| `docs/dev/git_workflow.md` | новый — канон конвенций | `930e323` |
| `CHANGELOG.md` | новый — Keep a Changelog `[Unreleased]` | `930e323` |
| `docs/architecture-spec.md` | секция «Stability contract» добавлена | `930e323` |
| `docs/architecture-spec.md` | убрана ссылка на workflow из спеки (post-hoc) | `f19c130` |
| `CLAUDE.md` | ссылка на git_workflow.md в Code style & AI constraints | `930e323` |
| `backlog/README.md` | feature-ветка как опция в шаге 2 «Старт работы» | `930e323` |
| `.claude/commands/commit.md` | переписан под Conventional Commits + scope таблицы | `f1d2629` |
| `.claude/commands/create-task.md` | секция «Когда начинаешь работу — выбор формата» | `f1d2629` |
| `backlog/wip → completed/007_*.md` | task closed | (текущий коммит) |

## Как работает

Процесс описывается единственным каноном — [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md). Все остальные документы (CLAUDE.md, backlog/README.md, скилы `/commit` и `/create-task`) ссылаются на него и не дублируют содержания.

**Слои:**
1. **Branching (GitHub Flow):** master всегда зелёный. Feature-ветки `<type>/<NNN>-<slug>` — опционально, для значимой работы. Admin репо в bypass list → direct push в master разрешён. Force-push заблокирован.
2. **Commits (Conventional Commits 1.0):** `<type>(<scope>): <subject>`. Type и scope-таблицы — в `git_workflow.md`, scope покрывает все подсистемы cpparch.
3. **Versioning (SemVer 2.0):** pre-1.0 `0.x.y` (breaking в MINOR допустим), после v1.0 — strict semver.
4. **Tags (annotated `vX.Y.Z`):** `git tag -a vX.Y.Z -m "Release X.Y.Z"`, push с `--follow-tags`.
5. **Changelog (Keep a Changelog 1.1):** [`CHANGELOG.md`](../../CHANGELOG.md) в корне, `[Unreleased]` сверху, при релизе фиксируется в `[X.Y.Z] - YYYY-MM-DD`.
6. **Stability contract:** что считается breaking (MAJOR) — таблица в [`docs/architecture-spec.md`](../../docs/architecture-spec.md) §Stability contract. Покрывает exit codes, CLI флаги, JSON-схему, SARIF, YAML-конфиг, baseline, дефолтные правила.
7. **AI-аудит трейлеры:** `AI-Assisted`, `Verified`, `Risk`, `Co-Authored-By` — поверх Conventional Commits, парсерами игнорируются.

## Чем управляется

- **`.claude/commands/commit.md`** — формат коммита enforce-ится скилом `/commit`.
- **`.claude/commands/create-task.md`** — формат имени задачи и совет по ветке.
- **GitHub Rulesets** (Settings → Rules → Rulesets для master):
  - Require pull request: ON, required approvals = 0.
  - Block force pushes: ON.
  - Bypass list: admin (`blurman-ai`).
- **GitHub Settings → General → Pull Requests**: Automatically delete head branches — стоит включить (опционально).

## С чем связана

- **#006 spec_refactor** — Stability contract попал в спеку как часть #007; шаг 5 #006 (closure двух-бекендного вопроса) уже частично сделан. Оставшиеся шаги #006: 1, 2, 3, 4, 6, 7, 8.
- **Все будущие задачи** — пользуются конвенциями `<type>/<NNN>-<slug>` для веток, Conventional Commits для коммитов, ссылаются `(#NNN)` в subject-е.
- **#002 github_actions_ci** — когда дойдём до CI, добавится commitlint в pre-merge checks (опционально, после первого реального contributor-а).

## Диагностика

Как понять, что процесс соблюдается:

- `git log --oneline | head` — все коммиты вида `<type>(<scope>): <subject>` или `<type>: <subject>` (если scope опущен). Не должно быть legacy-формата `<type>: [TAG] subject`.
- `git tag` — все теги имеют `v`-префикс и аннотацию (`git show <tag>` показывает сообщение).
- `cat CHANGELOG.md | head -20` — секция `[Unreleased]` существует, ссылки на keepachangelog.com и semver.org в шапке.
- `find backlog -name '???_*.md' | head` — все задачи именованы `NNN_<priority>_<name>.md`.
- `ls backlog/` — структура `new/ wip/ completed/`, никаких задач в корне.
- `git push origin master --force-with-lease` — должно отклоняться (force-push заблокирован для всех).
