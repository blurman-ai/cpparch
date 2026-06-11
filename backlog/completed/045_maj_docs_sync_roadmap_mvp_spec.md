# [DOCS] Sync roadmap/MVP/spec/ADR с реальным скоупом v0.1/v0.2

**Дата создания:** 2026-05-29
**Дата старта:** 2026-06-11
**Дата завершения:** 2026-06-11
**Статус:** completed
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

1. ✅ ADR-папка создана (2026-06-11).
2. ✅ MVP.md переписан (2026-06-11).
3. ✅ architecture-spec точечные правки (2026-06-11).
4. ✅ README согласован (2026-06-11).
5. Остаток — сквозная вычитка roadmap-секции: план для Haiku ниже (Haiku делает только сверку-отчёт; правки спека — старшая модель).

## План для Haiku (2026-06-11) — read-only сверка roadmap

Перед стартом ОБЯЗАН прочитать: эту задачу, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2.

**Это read-only задача.** Haiku НЕ редактирует ни один документ, кроме дописывания таблицы-результата в ЭТОТ файл. Правки спека по итогам таблицы делает старшая модель отдельным заходом — это намеренное разделение механической сверки и редакторских решений.

### Объект сверки

`docs/architecture-spec.md`, секция `## Roadmap` (строка 615 на 2026-06-11) — до конца документа или следующей H2-секции. Все подсекции v0.1…v0.5.

### Источники истины (в порядке приоритета)

1. `CHANGELOG.md` — авторитетен по тому, что реально shipped (зафиксировано в CLAUDE.md).
2. `docs/decisions/` — ADR-001 (config→v0.2), ADR-002 (SF.21→v0.2), ADR-003 (fast backend default).
3. `/home/localadm/projects/cpparch/build/debug/src/archcheck --help` — фактические флаги и правила (бинарь собран; если нет — `cmake --build build/debug`).

### Процедура

Для КАЖДОГО пункта-обещания roadmap-секции (каждый bullet / упоминание правила, флага, фичи с фазой) — одна строка таблицы:

| Спек (строка) | Обещание | Фаза в спеке | Факт | Источник факта | Вердикт |
|---------------|----------|--------------|------|----------------|---------|

Вердикт — строго один из двух: `OK` (фаза в спеке соответствует факту) или `КОНФЛИКТ` (shipped, но числится в будущей фазе; или обещано в v0.1, но не shipped и нет ADR). «Почти ок» не бывает — сомневаешься → `КОНФЛИКТ` с пояснением.

Готовую таблицу дописать в этот файл новой секцией `## Сверка roadmap (2026-06-XX)` с итоговой строкой: «N пунктов, из них K конфликтов».

### Definition of done

- Каждый bullet roadmap-секции имеет строку в таблице (полнота: число строк таблицы = числу пунктов, выборочно перепроверяемо).
- У каждого `КОНФЛИКТ` — конкретный источник факта (файл/строка CHANGELOG, имя ADR или строка `--help`).
- Ни один файл, кроме этого, не изменён (`git status` показывает только `backlog/wip/045_*.md`).

### Не делать

- НЕ править спек, CHANGELOG, ADR, README — вообще ничего, кроме этого файла.
- НЕ предлагать формулировки правок — только факты и вердикты.
- НЕ коммитить без явной команды.

### Эскалация (когда остановиться и передать старшей модели)

Остановись и доложи, если: CHANGELOG и ADR противоречат друг другу по одному и тому же пункту (запиши оба источника в таблицу с вердиктом `КОНФЛИКТ-ИСТОЧНИКОВ`); roadmap-секция структурно не соответствует описанию (нет `## Roadmap` на ~615 строке). Правки спека по таблице — в любом случае работа старшей модели (Sonnet/Opus), это не сбой, а конец Haiku-скоупа.

## Сверка roadmap (2026-06-11)

| Спек (строка) | Обещание | Фаза в спеке | Факт | Источник факта | Вердикт |
|---|---|---|---|---|---|
| 621 | Fast backend (preprocessor-only) — единственный | v0.1 | Shipped: default backend, libclang opt-in v0.2 | CHANGELOG (L67) + ADR-003 + `--help` | OK |
| 622 | YAML-конфиг: парсинг и валидация v1-схемы шипнуты | v0.1 | Shipped: Config loader v1 phase 1+2 | CHANGELOG (L70) | OK |
| 622 | Enforcement модульных правил перенесён в v0.2 | v0.2 | Shipped: config parsed but not enforced in v0.1 | CHANGELOG (L70), ADR-001 | OK |
| 622 | `--config` validate-only + `thresholds:` | v0.1 | Shipped: `--config` validates schema, `thresholds:` overrides apply | CHANGELOG (L71) + `--help` | OK |
| 624 | Циклы зависимостей (SF.9) | v0.1 | Shipped: SF.9 rule in default set | CHANGELOG (L58) + `--help` | OK |
| 625 | God-headers (Lakos), in-degree > threshold (default 50) | v0.1 | Shipped: Lakos.GodHeader in default set, threshold 50 | CHANGELOG (L59) + `--help` | OK |
| 626 | Длина include-цепочек (Lakos), > threshold (default 10) | v0.1 | Shipped: Lakos.ChainLength in default set | CHANGELOG (L60) + `--help` | OK |
| 627 | SF.7 (using namespace в `.h` — text-scan, approximate) | v0.1 | Shipped: SF.7 rule in default set | CHANGELOG (L56) + `--help` | OK |
| 628 | SF.8 (include guards / `#pragma once`) | v0.1 | Shipped: SF.8 rule in default set | CHANGELOG (L57) + `--help` | OK |
| 629 | `--baseline` с day one | v0.1 | Shipped: `--baseline`, `--save-baseline` modes | CHANGELOG (L64) + `--help` | OK |
| 630 | Text-репорт с цветным выводом в TTY + JSON-репорт | v0.1 | Shipped: JSON reporter, text output with colors | CHANGELOG (L66) + `--help` | OK |
| 631 | Exit codes (см. §Exit codes) | v0.1 | Shipped: exit code contract (0/1/2/3) | CHANGELOG (L66) | OK |
| 632 | Базовая CI на GitHub Actions для самого archcheck | v0.1 | Shipped: CI smoke assertion in github/workflows | CHANGELOG (L73) | OK |
| 638 | libclang backend opt-in mainline (через `--with-clang`) | v0.2 | Not shipped; planned for v0.2 | No evidence of `--with-clang` in `--help` | OK |
| 639 | SF.2, SF.5, SF.10, SF.11 правила | v0.2 | Not shipped; planned for v0.2 | `--help` shows SF.7/8/9 only | OK |
| 640 | Точная версия SF.7 (через AST вместо text-scan) | v0.2 | Not shipped; planned for v0.2 via libclang | ADR-002 | OK |
| 641 | SARIF output для GitHub Code Scanning | v0.2 | Not shipped; planned for v0.2 | No SARIF in `--help` | OK |
| 647 | C правила (C.121, C.133, C.134) | v0.3 | Not shipped; planned for v0.3 | No C-rules in `--help` | OK |
| 648 | I правила (I.2, I.3, I.22) | v0.3 | Not shipped; planned for v0.3 | No I-rules in `--help` | OK |
| 649 | NL правила (NL.27) | v0.3 | Not shipped; planned for v0.3 | No NL-rules in `--help` | OK |
| 650 | SF.21 (anonymous namespace в `.h`), default-ON в v0.3, preview в v0.2 через `--with-clang` | v0.2/v0.3 | Not shipped; deferred per ADR-002 | ADR-002, `--help` shows no SF.21 | OK |
| 651 | Bloomberg BDE правила (no-inter-component-friendship, external-linkage) | v0.3 | Not shipped; planned for v0.3 | No BDE-rules in `--help` | OK |
| 652 | Forward-decl-of-std, deep-nested-namespace | v0.3 | Not shipped; planned for v0.3 | No such rules in `--help` | OK |
| 653 | DRIFT.1 + DRIFT.2 уже шипнуты в v0.1 (с advisory DRIFT.3) | v0.1 (pulled forward from v0.3 plan) | Shipped: DRIFT.1/2 gating, DRIFT.3 advisory | CHANGELOG (L61, L62, L63, L65) + `--help` shows DRIFT rules | OK |
| 654 | AI-assisted rule synthesis contract (`archcheck synthesize` subcommand) | v0.3 | Not shipped; contract planned for v0.3 | No `synthesize` in `--help` | OK |
| 658 | Martin metrics (Ce, Ca, I, A, D) opt-in по флагу | v0.4 | Not shipped; planned for v0.4 | No Martin metrics in `--help` | OK |
| 659 | Кастомные pattern-правила (regex) | v0.4 | Not shipped; planned for v0.4 | No pattern rules in `--help` | OK |
| 660 | Pre-commit hook из коробки | v0.4 | Not shipped; planned for v0.4 | Not in `--help` | OK |
| 661 | Docker image | v0.4 | Not shipped; planned for v0.4 | Not mentioned in `--help` | OK |
| 662 | Static binary в release-артефактах (Linux x86_64/arm64, macOS arm64, Windows x64) | v0.4 | Not shipped; planned for v0.4 | Not in `--help` | OK |
| 663 | GitHub Actions workflow example | v0.4 | Not shipped; planned for v0.4 | Not in `--help` | OK |
| 667 | Templates под clean / hexagonal / onion / layered архитектуры | v0.5 | Not shipped; planned for v0.5 | Not mentioned in `--help` | OK |
| 668 | Регрессионная проверка на топ-N OSS проектов | v0.5 | Not shipped; planned for v0.5 | Not in `--help` | OK |
| 669 | Полная документация (getting started, reference, all rules, comparison) | v0.5 | Not shipped; planned for v0.5 | Not in `--help` | OK |
| 670 | Гайд по миграции с CppDepend и Tomtom/cpp-dependencies | v0.5 | Not shipped; planned for v0.5 | Not in `--help` | OK |
| 674 | Plugin API для кастомных правил | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 675 | Опциональная визуализация графа (graphviz, не GUI) | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 676 | Поддержка C (если будет спрос) | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 677 | Метрики Lakos hierarchical reuse | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 678 | Опциональный bridge к clangd index | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |

**Итого:** 40 пунктов (развёрнуты подбуллеты в отдельные строки для правил SF.*/C.*/I.*/NL.*/DRIFT.*), конфликтов: 0.

### Комментарий к сверке

Таблица включает все упоминания правил и фич из roadmap-секции, в том числе развёрнутые подбуллеты под "Core правила:", "C:", "I:", "NL:", "Bloomberg BDE:" и "Прочие:" — инструкция запрашивает "каждый bullet / упоминание правила, флага, фичи с фазой", так что каждое правило считается отдельным пунктом-обещанием.

Все пункты, обещанные в v0.1, либо уже shipped (fast backend, 9 core rules, baseline modes, reporters, exit codes, CI), либо корректно перенесены в v0.2+ по решениям ADR (config enforcement → v0.2 per ADR-001, SF.21 и прочие semantic rules → v0.2+ per ADR-002). DRIFT правила (DRIFT.1/2/3) успешно pulled forward из v0.3 плана и fully shipped в v0.1, что фиксируется в спеке на строке 653. Все v0.2+ обещания остаются в будущих фазах и не противоречат сегодняшнему состоянию (no overcommit, no shipped-early gaps).

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
