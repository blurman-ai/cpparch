# [DOCS] README → shipped reality (CLI + features)

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-29
**Дата завершения:** 2026-05-29
**Статус:** done
**Модуль:** DOCS
**Приоритет:** critical
**Сложность:** S (по факту: ~1 час с smoke-testом всех команд)
**Блокирует:** —
**Заблокирован:** —
**Related:** #6 (gh — audit Issues 1+2), #028 (rules_engine_mvp — зафиксировал v0.1 zero-config скоуп), #045 (docs_sync_roadmap_mvp_spec — внутренний срез той же синхронизации)

## Цель

Привести `README.md` в соответствие с тем, что реально шипится в текущем бинаре: убрать рекламу несуществующего CLI (`check --config`, `init`, `baseline create`) и невыпиленных фич (config-loader, SARIF), отделив их в раздел Planned/Roadmap.

## Контекст

Аудит #6 показал, что README ссылается на CLI и фичи, которые отложены в v0.2 решением #028 (config rules) или просто не реализованы (SARIF). Пользователь, копипастящий первый пример из README, падал на первой команде.

**Issue 1 — несуществующий CLI:**
- `archcheck check --config arch.yaml` — нет `check` subcommand, нет `--config`
- `archcheck init > arch.yaml` — нет `init`
- `archcheck baseline create --config ... --output ...` — нет `baseline create`
- Реальный CLI (см. `src/main.cpp` usage): `archcheck [path]` (zero-config), `--format json`, `--baseline`, `--save-baseline`, `--save-graph-baseline`, `--drift-baseline`, `--diff`, `--scan`, `--graph`.

**Issue 2 — невыпиленные фичи как present-tense:**
- «Applies declarative rules from a YAML config» — config loader отложен в v0.2 (#028).
- SARIF в Output formats — есть только `text_reporter` и `json_reporter`.

## Сделано

- **2026-05-29** — Сверка с реальным бинарём через `archcheck --help`. Каждая команда из переписанного README прогнана на `build/debug/src/archcheck`: bare check, `--format json`, `--save-baseline`/`--baseline` (full round-trip с подавлением), `--save-graph-baseline`/`--drift-baseline`, `--diff HEAD~1..HEAD`. Все 7 команд возвращают ожидаемые exit-коды и выводы.
- **2026-05-29** — Переписан README:
  - Раздел «What it does (today, v0.1)» — фактический список из пяти shipped правил (SF.7/8/9, Lakos.GodHeader порог 50, Lakos.ChainLength порог 10), drift и diff.
  - Раздел «Quick start» — реальные команды с zero-config first, baseline-флоу, drift и diff.
  - Пример output снят с прогона на `fixtures/sf9_no_cycles/fail_simple`: `a.h: [SF.9] cycle: a.h → c.h → b.h → a.h`.
  - Output formats — только text и json.
  - «What it is NOT» расширен под CLAUDE.md формулировки (clang-tidy / PVS-Studio / clang-format / IWYU явно).
  - Появился раздел «Planned (v0.2+)» — туда переехали config rules, `init`, `--with-clang`, SARIF, color TTY со ссылкой на спек и ADR-трейл.

## Как работает

README теперь имеет три чёткие зоны:

1. **«What it does (today, v0.1)»** — закрытый список того, что реально работает. Каждый bullet прогоняется на бинаре одной командой. Если bullet тут — он должен работать без сноски.
2. **«Quick start»** — семь команд, покрывающих три use-case: статический check, baseline-флоу (legacy adoption) и drift/diff (CI на PR).
3. **«Planned (v0.2+)»** — открытый список того, что обещано спекой, но не шипится. Каждый пункт ссылается на ADR или backlog — читателю видно, что фича не забыта, а отложена сознательно.

Главный invariant: **любая команда из «Quick start» должна работать на текущем бинаре без флагов сборки, не упомянутых в README.** При следующем релизе или удалении флага — README обновляется тем же коммитом.

Источник правды для shipped CLI — `archcheck --help` (см. [src/main.cpp](src/main.cpp)). При расхождении README ↔ `--help` — README ошибается, не бинарь.

## Чем управляется

Документ ничем не управляется в runtime. Из контрольных привязок:

- Список default rules в README должен совпадать с регистрацией в `src/rules/rule_set.cpp` (5 правил v0.1).
- Пороги в bullet «Lakos.GodHeader» (50) и «Lakos.ChainLength» (10) — копии `kDefaultThreshold` из [include/archcheck/rules/lakos_god_headers.h:14](include/archcheck/rules/lakos_god_headers.h#L14) и из lakos_chain_length.h соответственно. При изменении порога — обновить README той же правкой.
- Пример output для SF.9 в README — формат, который пишет `text_reporter.cpp`. При смене формата — пересобрать пример.

## С чем связана

- **#028** — закрытое решение «config rules → v0.2». README раздел Planned ссылается на него косвенно через ADR-трейл.
- **#045** — параллельный sync для MVP.md / architecture-spec.md / ADR. README сделан первым, чтобы #045 опирался на готовые тексты.
- **#046** — color TTY decision; пока трек не выбран, цвет упомянут как Planned.
- **#042 + #043** — `--with-clang` Planned. README не обещает сроков — только направление.

## Диагностика

- При сомнении «соответствует ли README бинарю» — `diff <(grep -E '^archcheck' README.md) <(archcheck --help | grep -E '^\s*archcheck')`. Грубо, но ловит расхождение в наборе флагов.
- Если новый пользователь упёрся в команду из Quick start — это регрессия README, а не баг бинаря. Восстанавливать через прогон команды на чистой машине и обновление README.
- Если в Planned появилось что-то, что фактически уже шипится — переехать в «What it does», обновить bullet, удалить из Planned.

## Изменённые файлы

| Файл | Изменение | Коммит |
|------|-----------|--------|
| `README.md` | rewrite: Usage / What it does / Output formats / Planned | (предстоит коммит) |
