# [CONFIG][V1] Минимальный контракт конфига для post-MVP фазы

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
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
| Открытый вопрос: allowlist vs forbidden | Deptrac — pure allowlist (безопаснее, permission creep невозможен); dep-cruiser — mixed. Для legacy-adopt `forbidden` проще. Решить до реализации loader-а |
| Stale suppressions alerting (v1 phase 2) | Import Linter: `unmatched_ignore_imports_alerting` — если suppress устарел, это тоже violation |
| `name` обязателен | Используется в violation output как machine-readable id |

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

- [ ] Написать `docs/config_format.md`: YAML-схема, поля, типы правил, примеры
- [ ] Зафиксировать answer на вопрос allowlist vs forbidden model (или дать оба варианта)
- [ ] 3-4 reference examples: tiny (2 modules), layered (3+ layers), legacy (только forbidden), mixed `include/`+`src/`
- [ ] Явно выписать что в scope v1 phase 1 и что нет (таблица)
- [ ] Описать backwards compatibility: `version: 1` — намеренный SemVer для schema
- [ ] После фиксации `docs/config_format.md` — завести implementation-task на `config/` loader

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/config_format.md | новая спецификация минимального формата |
| README.md | краткий reference example после стабилизации формата |
