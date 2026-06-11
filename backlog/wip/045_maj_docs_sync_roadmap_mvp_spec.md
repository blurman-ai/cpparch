# [DOCS] Sync roadmap/MVP/spec/ADR с реальным скоупом v0.1/v0.2

**Дата создания:** 2026-05-29
**Дата старта:** 2026-06-11
**Статус:** wip
**Модуль:** DOCS
**Приоритет:** major
**Сложность:** M (1–2 дня; MVP.md проще переписать с нуля)
**Блокирует:** —
**Заблокирован:** —
**Related:** #6 (gh — audit Issues 3, 4, 5, 9), #028 (rules_engine_mvp — решение про config→v0.2), #006 (spec_refactor — fast-backend default), #044 (docs_readme_sync_shipped — внешний срез той же синхронизации)

## Цель

Привести `docs/MVP.md`, `docs/architecture-spec.md` и логику decisions-доков к фактически принятому скоупу: config-правила и SF.21 отложены в v0.2 (#028), fast-backend — дефолт в v0.1 (#006), drift/git уже шипятся. Зафиксировать решения отдельно от backlog/completed, чтобы их видели читатели и агенты.

## Контекст

Аудит #6 показал четыре пересекающихся расхождения между документами и реальностью. Сам факт, что аудит долго не находил решение #028 (оно спрятано в `backlog/completed/`), — отдельный сигнал: deferral-решения должны быть видны там, где живёт роадмап, а не в архиве задач.

### Issue 3 — `docs/MVP.md` описывает отвергнутый pre-#006 дизайн

`docs/MVP.md` предшествует рефакторингу #006 и решению #028. Описывает архитектуру, которая отвергнута:
- Core Feature #1 = «`compile_commands.json` support», но #006 сделал препроцессорный fast-backend дефолтом и отложил libclang/`compile_commands` в v0.2.
- #4 Module Mapping и #5 Dependency Rules как core MVP — отложены в v0.2 (#028).
- Acceptance: «enforces 1 dependency rule»; Success: `archcheck check --config arch.yaml` — ничего из шипящегося zero-config продукта.

Документ протух end-to-end. Дешевле переписать с нуля (или удалить и сложить в `architecture-spec.md`), чем патчить по строкам.

### Issue 4 — `architecture-spec.md` roadmap всё ещё пишет config+SF.21 в v0.1

- Строка 636: «YAML-конфиг: `forbidden_deps` / `allowed_deps`» как v0.1 core — отложено в v0.2 (#028).
- Строка 163: SF.21 (approx text-scan) как v0.1 — отложено в v0.2 (#028); нет реализации.
- Строка 408 называет modules + `forbidden_deps`/`allowed_deps` «the thing users install archcheck for» — headline-фича, которая отложена. Спек фреймит отложенную фичу как core reason-to-exist.

### Issue 5 — Config→v0.2 решение похоронено в completed-таске

Обоснование живёт только в `backlog/completed/028_maj_rules_engine_mvp.md` (line 18 + decisions). Не выведено ни в roadmap, ни в `architecture-spec.md`, ни в `CLAUDE.md`. Читатель/агент, сравнивающий код с роадмапом, повторно выводит «core feature missing» — аудит сам в это попался.

### Issue 9 — Roadmap занижает поставленный прогресс

DRIFT.1/2 расписаны в v0.3, но drift-правила уже шипятся (`src/rules/drift_no_cycle_growth.cpp`, `drift_no_shortcut_edge.cpp`) вместе с инфраструктурой git/diff/graph-baseline (#009, #015–018, #022–025). Роадмап протух в обе стороны: drift приехал раньше, v0.1 config-headline так и не приехал (by design).

## План выполнения

### MVP.md (Issue 3)
- [ ] Принять решение: переписать с нуля или удалить и сложить в `architecture-spec.md`. По умолчанию — переписать.
- [ ] Новый `docs/MVP.md` декларирует: zero-config intrinsic rules, fast-backend only, no `compile_commands` required, config + AST rules = v0.2.
- [ ] Acceptance criteria и Success Condition отражают `archcheck [path]`, не `archcheck check --config`.

### architecture-spec.md (Issues 4 + 9)
- [ ] Удалить «YAML-конфиг: forbidden_deps/allowed_deps» из v0.1, перенести в v0.2 (строка 636).
- [ ] Переместить SF.21 из v0.1 в v0.2 в таблице правил (строка 163).
- [ ] Разрулить «headline value» противоречие (строка 408): зафиксировать, что v0.1 = intrinsic-ценность (cycles, god-headers, chains), а config-contract headline уезжает в v0.2.
- [ ] Подтянуть drift/git в роадмап как «delivered in v0.1 (pulled forward from v0.3)» — таски #015–018, #022–025, #009.

### ADR / decisions (Issue 5)
- [ ] Решить место: `docs/decisions/` (ADR per file) или секция `## Принятые решения` в `architecture-spec.md`. По умолчанию — отдельная папка `docs/decisions/`, легче расширять.
- [ ] ADR-001 «config rules → v0.2» (entry barrier, zero-config adoption, ссылка на #028).
- [ ] ADR-002 «SF.21 → v0.2» (требует libclang; text-scan ненадёжен, ссылка на #028 и #042).
- [ ] ADR-003 «compile_commands optional, fast backend default» (#006).
- [ ] Линк из `CLAUDE.md` § «Design docs» на `docs/decisions/`, чтобы агенты видели.

## Критерий приёмки

- Любое правило / фича упомянуто ровно в одной фазе роадмапа (нет конфликтующих статусов).
- Решения про deferral discoverable из спека и `CLAUDE.md` без чтения `backlog/completed/`.
- `MVP.md` соответствует тому, что реально шипится сегодня.

## Сделано

- **2026-06-11** (срез по запросу владельца — актуализация MVP):
  - `docs/MVP.md` переписан с нуля: zero-config критерии приёмки (9 пунктов с фактическими
    статусами), non-goals со ссылками на решения, success condition = 4 команды без YAML,
    секция «Why these criteria» с evidence из исследования. «enforces 1 dependency rule»
    убран как противоречащий решению #028.
  - Создана `docs/decisions/`: ADR-001 (config→v0.2, усилен данными 2026-06: Figma/Chrome
    прецедент, JetBrains 30% без анализа), ADR-002 (SF.21→v0.2), ADR-003 (fast backend
    default, числа спайка #043 + clang-tidy/clangd референсы).
  - architecture-spec: «конфиг — то, ради чего ставят archcheck» (Issue 4, бывш. стр. 408)
    переформулирован как v0.2-headline со ссылкой на ADR-001. SF.21 (стр. 173) и roadmap
    drift pull-forward были закрыты ранее правками v2.2 (2026-06-11).
  - README: статус-строка ссылается на ADR-001 и MVP.md; открытый release-итем = #105.
  - CLAUDE.md: ссылка на docs/decisions/ в Design docs («сверяйся перед объявлением гэпа»).

## В работе

- Остаток: решить судьбу старых формулировок Issue 9 в спеке (drift pull-forward отмечен,
  но сквозную вычитку roadmap-секции стоит сделать одним проходом перед v0.1-тегом).

## Следующие шаги

1. Сначала ADR-папка (даёт стабильный якорь для ссылок из других правок).
2. Потом MVP.md (один проход).
3. Потом architecture-spec.md (точечные правки + drift pull-forward).
4. Согласовать формулировки с #044 (README) — там тот же deferral, не разойтись.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Один зонтик-таск, а не четыре | findings 3/4/5/9 — один deferral-нарратив, дробить = повторять контекст 4 раза |
| ADR в `docs/decisions/`, а не в спеке | расширяемо, не раздувает спек, легче ссылаться |
| MVP.md перепис, а не патч | стало быстрее с нуля, чем синхронизировать постранично |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `docs/MVP.md` | rewrite (или delete + fold в spec) |
| `docs/architecture-spec.md` | удалить config-rules и SF.21 из v0.1, разрулить headline-конфликт, поднять drift до delivered |
| `docs/decisions/0001-config-rules-v02.md` | new ADR |
| `docs/decisions/0002-sf21-v02.md` | new ADR |
| `docs/decisions/0003-fast-backend-default.md` | new ADR |
| `CLAUDE.md` | ссылка на `docs/decisions/` в § Design docs |
