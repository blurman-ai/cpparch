# [RULES][SCAN][REPORT] Clone detection pain points & opportunities (LD.10–LD.18)

**Дата создания:** 2026-06-02
**Дата старта:** —
**Статус:** new
**Модуль:** RULES / SCAN / REPORT
**Приоритет:** minor
**Сложность:** unknown (umbrella — каждый LD.* оценивается отдельно)
**Целевой релиз:** v0.3+
**Блокирует:** —
**Заблокирован:** —
**Related:** #052 (cross_tu_duplication_detector), #053 (line_duplication_pass), #056 (token_duplication_pass), #059 (coincidental_clone_filtering), #054 (ai_repo_duplication_run), #065 (dedup_skip_generated), #070 (FP-фиксы чекера), #033 (ai_drift_dataset)
**Spin-off:** #072 (clone_type_classifier — LD.10 + LD.11 реализованы в спайке #056)

## Цель

Собрать в один исследовательский бэклог болевые точки CPD-style клон-детекторов и возможности, которые отличали бы archcheck от «нашёл дублированный блок» — классификация, объяснение, категории, метрики роста и архитектурный срез дублирования.

## Контекст

Источники-инспирация: обсуждения SonarSource Community, вопросы StackOverflow, исследования по clone detection, общие жалобы на CPD-tools.

Статус — **future ideas, не MVP**. Это надстройка над уже существующей подсистемой дубликатов: комплементарные слои (#053 line / #056 token / #052 AST / #059 precision / #054 usage). Единый источник истины — [docs/duplication_architecture.md](../../docs/duplication_architecture.md); читать перед работой над любым LD.* пунктом. Часть пунктов прямо продолжает уже идущую работу по precision/FP (#059, #065, #070) — их стоит снимать как «бесплатные» побочные эффекты той ветки.

Перед тем как тащить любой пункт в реализацию — проверить против списка «What it explicitly is NOT» в [CLAUDE.md](../../CLAUDE.md): не превращаем archcheck в GUI/дашборд, остаёмся CLI+CI.

### Каталог идей

**LD.10 — Clone Classification.** Детекторы рапортуют «duplicated block», но не различают тип: exact / identifier-renamed / literal-only / structural. Без типа разработчик не понимает, опасен клон или безвреден. Выход: `Clone Type: Exact|Identifier-Renamed|Literal-Only|Structural`. Ценность: меньше FP-жалоб, проще триаж и приоритизация. (У нас уже есть selective normalization в токеновом проходе — тип клона во многом выводится из того, *что* было занормализовано при матче.)

**LD.11 — Clone Explanation.** Тулы показывают «Duplicated block detected» без объяснения *почему*. Разработчики спорят с результатом, не понимая, что совпало. Выход: matched tokens, ignored identifiers (`userId -> accountId`), ignored literals (`100 -> 200`), similarity %. Ценность: доверие к результату, отладка логики детекции, проще adoption. **High interest.**

**LD.12 — Clone Categories.** Не все клоны одинаково вредны: generated code, DTO, protobuf-структуры, ORM-сущности, lookup-таблицы триггерят отчёты, но редко = архдолг. Категории: Generated / Data Mapping / Test Code / Business Logic / Unknown. Ценность: signal-to-noise, фокус на реальной стоимости сопровождения. (Пересекается с #065 dedup_skip_generated — там generated уже отсекается; здесь — расширение до классификации, а не только skip.) **Research required.**

**LD.13 — Micro-Clones.** Большие пороги прячут мелкие дубли: validation, retry, logging, exception handling. Выход: `Micro Clone / Lines: 5 / Occurrences: 7`. Ценность: ранняя детекция, refactoring opportunities, полезно для AI-сгенерированного кода. **Research required** (риск взрыва FP при низком пороге — требует осторожной валидации на корпусе, ср. #059/#066).

**LD.14 — Clone Growth.** Тулы отвечают «сколько дублирования есть», но не «сколько добавлено». Метрики: Clone LOC Delta, Clone Density Delta, Clone Block Delta. Пример: `Before 3.1% → After 3.8% → Delta +0.7%`. Ценность: CI-гейтинг, отслеживание тренда, сигнал архитектурного дрейфа. (Ложится на существующие baseline/drift/diff-режимы бинаря.) **High interest.**

**LD.15 — Diff-Induced Clones.** «Откуда пришёл этот клон?» — тулы редко связывают клоны с историей изменений. Выход: `Clone introduced in: commit abc123`, source/target файлы. Ценность: accountability, проще ревью, root-cause. (Требует git-blame интеграции — проверить против «not a GUI»; это CLI-обогащение отчёта, допустимо.) **High interest.**

**LD.16 — Cross-Module Clone Matrix.** Дублирование через границы подсистем опаснее внутримодульного. Выход: матрица `Navigation -> Weather: 320 LOC`, `Navigation -> Licensing: 140 LOC`. Ценность: сигнал архдрейфа, видимость связности подсистем, полезно для больших реп. (Прямо в духе Lakos physical design — самая «архитектурная» из идей, естественный фит позиционированию.) **High interest.**

**LD.17 — Clone Hotspots.** Некоторые файлы постоянно становятся источниками клонов. Выход: `Top Clone Sources: config.cpp exported 14 clones`. Ценность: цели для рефакторинга, отслеживание эрозии. **Medium interest.**

**LD.18 — AI Clone Fingerprints.** AI-код плодит похожие паттерны по всей репе; традиционная детекция видит симптомы, но не паттерн. Выход: `Repeated Pattern: RetryLoopV1 / Occurrences: 17 / Modules: 8`. Ценность: мониторинг AI-assisted разработки, drift-анализ, гигиена репозитория. (Связь с #054 ai_repo_duplication_run и #033 ai_drift_dataset — duplication = №1 AI-drift сигнал.) **Research required.**

### Приоритеты (из источника)

- **High interest:** LD.11 (explanation), LD.14 (growth), LD.15 (diff-induced), LD.16 (cross-module matrix).
- **Medium interest:** LD.10 (classification), LD.17 (hotspots).
- **Research required:** LD.12 (categories), LD.13 (micro-clones), LD.18 (AI fingerprints).

## План выполнения

- [ ] Перечитать [docs/duplication_architecture.md](../../docs/duplication_architecture.md) и зафиксировать, что из LD.* выводится «бесплатно» из текущего конвейера (тип клона, matched tokens, ignored ids/literals — уже считаются при матче).
- [ ] Для каждого High-interest пункта (LD.11/14/15/16) — отдельная задача в `backlog/future/` с фикстурами и оценкой сложности, когда дойдёт черёд.
- [ ] LD.12/13/18 — сначала research-заметка (риски FP, объём корпуса), потом решение go/no-go.
- [ ] Проверить каждый кандидат против «What it is NOT» (CLI+CI, не дашборд) перед промоушеном из future/.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. ~~LD.10 (classification) + LD.11 (explanation)~~ — **сделано в спайке #056** (см. #072): данные были в матчере, классификатор + explain поверх `rawSeq`/`diffTokens`.
2. LD.14 (growth) — приземлить на существующие baseline/drift-режимы.
3. LD.16 (cross-module matrix) — самый «Lakos-родной» пункт, кандидат на флагманскую фичу v0.3.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Один umbrella-таск, не 9 отдельных | Future research; дробить на задачи по мере промоушена High → Medium → Research |
| Положить в `future/`, релиз v0.3+ | Явная директива «для будущего на исследование»; подсистема дубликатов ещё в v0.1/v0.2 (#052–#070) |
| LD.11/10 выводятся из матчера, не новая инфра | selective normalization уже знает matched tokens + ignored ids/literals |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| (пока нет) | research-фаза |

## Fixtures (если правило)

- [ ] `fixtures/clone_classification/` — exact / renamed / literal-only / structural (LD.10)
- [ ] `fixtures/clone_explanation/` — пары с известными ignored ids/literals (LD.11)
- [ ] `fixtures/clone_growth/` — before/after снапшоты для delta-метрик (LD.14)
- [ ] `fixtures/cross_module_clones/` — дубли через границы модулей (LD.16)
