# [CONFIG][V1] Минимальный контракт конфига для post-MVP фазы

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-29
**Дата завершения:** 2026-05-29
**Статус:** done
**Модуль:** CONFIG
**Приоритет:** major
**Сложность:** S (спека и примеры, без реализации)
**Целевой релиз:** v1 phase 1 (post-MVP)
**Блокирует:** реализацию config-loader после MVP
**Заблокирован:** —
**Related:** docs/architecture-spec.md, docs/MVP.md, future/v1_maj_agent_config_authoring_rules.md

## Цель

Зафиксировать минимальный формат `.archcheck.yml` — только `version`, `modules`, `rules`.
Никаких pattern-rules, severity per rule, inheritance, collectors, tags, policy engine.
Нужен документ до кода, иначе формат расползётся по README, спекам и loader-у.

## Целевой shape

```yaml
version: 1

modules:
  domain:
    paths: ["src/domain/**"]   # glob only, без regex
  app:
    paths: ["src/app/**"]
  infra:
    paths: ["src/infra/**"]

rules:
  # Слоистая архитектура: layers[0] может видеть всё ниже, layers[-1] не видит никого выше
  - type: layers
    name: main-layering
    layers: [app, domain, infra]   # высший → низший (Lakos levelization)

  # Точечный запрет (escape hatch или уточнение поверх layers)
  - type: forbidden
    name: domain-no-infra
    from: [domain]
    to: [infra]

  # Взаимная независимость: модули одного уровня не должны знать друг о друге
  - type: independence
    name: parallel-modules-isolated
    modules: [domain, infra]
```

**Что в v1 phase 2, не phase 1:** `ignore`, `baseline`, `required`, fan-in threshold,
`auto_modules` (pattern-based slice discovery), `protected` type, `severity` per rule.

## Дизайн-решения

| Решение | Причина |
|---------|---------|
| `layers` как основной тип | Один контракт заменяет N×(N-1)/2 пар `forbidden`; прямо воплощает Lakos levelization |
| `independence` отдельным типом | Горизонтальная независимость (один уровень) нужна отдельно от вертикальной иерархии |
| glob paths, без regex | Regex в module membership слишком мощен и непредсказуем для v1 |
| allowlist vs forbidden → **mixed by rule type** (2026-05-29) | `layers` / `independence` = implicit allowlist (строгость там, где есть контракт); `forbidden` = explicit blocklist (escape hatch для legacy и surgical override). Глобально выбирать не надо — пользователь миксует per-rule. Модель Import Linter. |
| Stale suppressions alerting (v1 phase 2) | Import Linter: `unmatched_ignore_imports_alerting` — если suppress устарел, это тоже violation |
| `name` обязателен | Используется в violation output как machine-readable id (`[rule:<name>]`) |

## Референсы (прочитаны, ссылки живые на 2026-05-28)

| Инструмент | Что взять | Ссылка |
|------------|-----------|--------|
| Deptrac | Shape: modules → ruleset; skip_violations как baseline | https://deptrac.github.io/deptrac/configuration/ |
| Import Linter | Typed contracts; `layers`, `independence`, `forbidden`, `protected` types; `ignore_imports` с wildcards | https://import-linter.readthedocs.io/en/stable/contract_types/ |
| dependency-cruiser | Rule buckets `forbidden`/`allowed`/`required`; `numberOfDependentsMoreThan` (god-headers); `moreUnstable` (Martin SDP); group matching $1 | https://github.com/sverweij/dependency-cruiser/blob/main/doc/rules-reference.md |
| ArchUnit | `slices().matching("pkg.(*)..")` → идея auto_modules (v1 phase 2) | https://www.archunit.org/userguide/html/000_Index.html |
| Nx | tags как вторичный слой поверх path-based modules (future) | https://nx.dev/docs/features/enforce-module-boundaries |
| Bazel | `package_group` — именованные группы модулей для переиспользования в rules | https://bazel.build/concepts/visibility |

## План выполнения

- [x] Написать `docs/config_format.md`: YAML-схема, поля, типы правил, примеры
- [x] Зафиксировать answer на вопрос allowlist vs forbidden model
- [x] 3-4 reference examples: tiny (2 modules), layered (3+ layers), legacy (только forbidden), mixed `include/`+`src/`
- [x] Явно выписать что в scope v1 phase 1 и что нет (таблица)
- [x] Описать backwards compatibility: `version: 1` — намеренный SemVer для schema
- [x] Синхронизировать `docs/architecture-spec.md` §«Анализ по конфигу» (старый формат → pointer на новый док)
- [x] Завести implementation-task на `src/config/` loader — `backlog/new/051_maj_config_loader_v1.md`

## Сделано

- 2026-05-29: написан `docs/config_format.md` — single source of truth для `.archcheck.yml` v1.
  - Three top-level keys: `version` / `modules` / `rules`.
  - Three rule types: `layers`, `independence`, `forbidden` — semantics + полные поля каждого.
  - Четыре reference-примера: tiny (2 модуля) / layered (3 слоя) / legacy (только forbidden) / mixed (`include/` + `src/` + `layers` + `independence` + `forbidden` в одном конфиге).
  - Таблица "что в scope phase 1 и что нет" (`defaults`, `thresholds`, `baseline`, `ignore`, `required`, `protected`, `severity`, `auto_modules`, тэги Nx, `package_group` Bazel, pattern-правила — всё отложено или dropped).
  - SemVer-контракт схемы: MINOR/MAJOR таблица, schema version независим от версии бинаря.
  - Diagnostics contract: `[rule:<name>]` в text/JSON output — стабильный machine-readable id, переименование = breaking change для baseline.
- 2026-05-29: зафиксирован ответ на allowlist vs forbidden — **mixed by rule type** (модель Import Linter):
  - `layers` / `independence` — implicit allowlist (строгость там, где нужен контракт).
  - `forbidden` — explicit blocklist (escape hatch для legacy и surgical override).
  - Глобальный выбор не нужен: пользователь миксует per-rule в одном конфиге.
- 2026-05-29: `docs/architecture-spec.md` §«Анализ по конфигу» сокращён: старая портянка (`module: X, forbidden_deps`, pattern-правила, `defaults`, `thresholds` в одном куске) удалена; оставлен короткий минимальный пример + ссылка на `docs/config_format.md` как single source of truth.

## Открытые вопросы / follow-up

- **Следующая задача:** [`backlog/new/051_maj_config_loader_v1.md`](../new/051_maj_config_loader_v1.md) — implementation на `src/config/` loader (YAML → `Config` struct, валидация по `docs/config_format.md`, line-numbered errors, fixtures: 4 pass + 9 fail).
- **README.md config example** синхронизировать **после** того, как loader подтвердит схему на реальном репо — не сейчас, иначе риск разъехаться с реальным поведением до первого end-to-end прогона.
- **v1 phase 2** (`defaults`, `thresholds`, `baseline`, `ignore`) — отдельная спека, после того как phase 1 загружается loader-ом и проходит fixtures.
- **Открытые вопросы внутри phase 1**: на момент закрытия не осталось — все дизайнерские развилки разрешены в `docs/config_format.md`.

## Изменённые файлы

| Файл | Изменение | Коммит |
|------|-----------|--------|
| docs/config_format.md | новая спецификация минимального формата (создан) | `4a14717` |
| docs/architecture-spec.md | §«Анализ по конфигу»: старый формат заменён на короткий пример + pointer | `4a14717` |

## Как работает

Контракт разделён на два слоя:

1. **`docs/config_format.md`** — авторитативный источник формата. Описывает три top-level ключа (`version` / `modules` / `rules`), три типа правил (`layers` / `independence` / `forbidden`), их семантику, диагностический формат (`[rule:<name>]`) и SemVer-контракт схемы. Имеет четыре reference-примера, по которым loader (#051) валидируется fixtures-ами.
2. **`docs/architecture-spec.md` §«Анализ по конфигу»** — короткий обзор с указателем на (1). Не дублирует формат, не разъезжается во времени.

Ключевая идея — **typed contracts**: тип правила определяет семантику (`layers`/`independence` — implicit allowlist, `forbidden` — explicit blocklist), пользователь миксует per-rule в одном конфиге. Глобальный выбор "allowlist project vs blocklist project" не нужен. Модель скопирована с Import Linter.

`name` обязателен у каждого правила — он машинно-читаемый id в диагностике (`[rule:<name>]`), и переименование = breaking change для baseline (это сознательно).

## Чем управляется

- **Авторитет:** `docs/config_format.md` — single source of truth. Любой другой документ (spec, README, AI agent prompt) ссылается сюда, не дублирует формат.
- **Эволюция:** SemVer внутри схемы. Добавление top-level ключа с default-значением — MINOR (всё ещё `version: 1`). Удаление ключа / смена семантики — MAJOR (`version: 2`). Архcheck-binary читает любой `version: 1` весь свой lifetime.
- **Расширение в phase 2:** `defaults`, `thresholds`, `baseline`, `ignore` — добавляются как новые ключи, без поломки phase 1 конфигов.

## С чем связана

- **Производит:** контракт, по которому пишется loader → [`#051`](../new/051_maj_config_loader_v1.md).
- **Разблокирует:** [`v1_maj_agent_config_authoring_rules.md`](../future/v1_maj_agent_config_authoring_rules.md) — без формата AI-агенту не было целевой формы для `.draft`. Эта зависимость снимается **после** #051 (агенту нужен работающий loader, чтобы валидировать вывод).
- **Замещает:** §«Анализ по конфигу» в `architecture-spec.md` v2.1 (старый формат `module: X, forbidden_deps: [Y]` + pattern-правила + `defaults` + `thresholds` в одной портянке). Старая секция теперь pointer.
- **Соседствует с:** [`#010`](../future/010_maj_ai_rule_synthesis_contract.md) — старый общий synthesize-контракт (CLI shape, heuristic vs wrapper-prompt). #010 шире (про CLI и режимы), эта задача — про сам формат YAML, который synthesize должен производить.

## Диагностика

Если эта задача "разъехалась" с реальностью, симптомы и где смотреть:

- **Конфиг loader (#051) валидирует не то, что в спеке** — править `docs/config_format.md` (это контракт), потом синхронизировать loader. Не наоборот.
- **README показывает старый формат** — он намеренно не синхронизирован, README sync ждёт end-to-end прогона через loader (см. follow-up выше).
- **AI-агент пишет config в произвольной форме** — значит [`v1_maj_agent_config_authoring_rules.md`](../future/v1_maj_agent_config_authoring_rules.md) ещё не реализована, агент не знает про targeted формат. Это нормально до того, как #051 закроется.
- **Кто-то предлагает добавить `defaults`/`severity`/pattern-правила в phase 1** — отказывать, это phase 2 или dropped, причины зафиксированы в таблице "What is **not** in v1 phase 1" в `docs/config_format.md`.
- **Конфликт между `architecture-spec` §«Анализ по конфигу» и `docs/config_format.md`** — `config_format.md` побеждает. Spec — обзорный документ.
