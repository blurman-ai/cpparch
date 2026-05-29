# [DOCS] README → shipped reality (CLI + features)

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** DOCS
**Приоритет:** critical
**Сложность:** S (полдня)
**Блокирует:** —
**Заблокирован:** —
**Related:** #6 (gh — audit Issues 1+2), #028 (rules_engine_mvp — зафиксировал v0.1 zero-config скоуп)

## Цель

Привести `README.md` в соответствие с тем, что реально шипится в текущем бинаре: убрать рекламу несуществующего CLI (`check --config`, `init`, `baseline create`) и невыпиленных фич (config-loader, SARIF), отделив их в раздел Planned/Roadmap.

## Контекст

Аудит #6 показал, что README ссылается на CLI и фичи, которые отложены в v0.2 решением #028 (config rules) или просто не реализованы (SARIF). Пользователь, копипастящий первый пример из README, падает на первой команде.

**Issue 1 — несуществующий CLI:**
- `archcheck check --config arch.yaml` (README.md:63, 91, 99) — нет `check` subcommand, нет `--config`
- `archcheck init > arch.yaml` (README.md:93) — нет `init`
- `archcheck baseline create --config ... --output ...` (README.md:95–97) — нет `baseline create`
- **Фактический CLI** (`src/main.cpp` usage block 39–47): `archcheck [path]` (zero-config), `--format json`, `--baseline <file>`, `--graph`, `--scan`, `--save-baseline`, drift/diff.

**Issue 2 — невыпиленные фичи как present-tense:**
- «Applies declarative rules from a YAML config» (README.md:27) — config loader отложен в v0.2 (#028), не реализован.
- SARIF в Output formats — есть только `text_reporter` и `json_reporter`; `sarif_reporter` отсутствует в `src/report/`.

## План выполнения

- [ ] Проверить актуальный usage-блок `src/main.cpp` — выписать ровно те флаги/субкоманды, которые шипятся сегодня
- [ ] Переписать раздел Usage / примеры в `README.md` под shipped CLI (`archcheck [path]`, `--baseline <file>`, `--format json`)
- [ ] Переписать «What it does» — описать только реализованные intrinsic-правила (SF.7/8/9, Lakos god-headers, Lakos chain-length, drift-rules), text + JSON output
- [ ] Завести раздел `## Planned / Roadmap` (или однозначно помеченный «v0.2»), куда переехать config rules, SARIF, `check --config`, `init`, `baseline create`
- [ ] Сверить с `docs/architecture-spec.md` v0.1 секцией, чтобы не разойтись
- [ ] Проверить каждую команду из примеров README на реальном бинаре — должна работать без ошибки

## Критерий приёмки

Каждая команда, показанная в README, выполняется как написано на текущем бинаре, либо явно помечена как «Planned (v0.2)».

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Прочитать актуальный `src/main.cpp` usage block.
2. Согласовать формулировки с #045 (общий sync роадмапа), чтобы не дублировать.
3. Прогнать примеры через `./build/debug/src/archcheck`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Сначала README, потом MVP/spec (#045) | README — единственный документ, который видит внешний пользователь; ставка на zero-config адопшен ломается, пока он лжёт |
| Отдельный раздел Planned, а не удаление | сохраняет позиционирование v0.2 фич, не выглядит как «продукт сдулся» |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `README.md` | переписать Usage / What it does / Output formats; добавить Planned/Roadmap |
