# Backlog Task Tracker — MVP v1

_2026-06-13 (третий проход); синхронизировано с backlog_review.md. #073 и #075 закрыты (completed 2026-06-12). Новый P0 блокер: #123 (new-clone-gate). Известный ID-clash: #117 (два файла в completed — diff\_max\_added\_lines + lateral\_backedge\_confirm), #118 (completed drift4\_lateral + new diff\_max\_added\_lines — **вероятный дубль**, diffMaxAddedLines уже реализован в коде). Долг: #071 (completed + future), #029 в new/ (реализован c480e39), #109 closed._

## Что считаем MVP v1

`archcheck` MVP v1 = **trusted dependency diff для C++ PR**:

- zero-config first;
- один понятный diff workflow для CI;
- детерминированный `text/json` output;
- честные CLI/док-контракты;
- trusted graph signal без известных грубых false positive в дефолтном режиме.

Это **не** duplication-gate, не AI-attribution и не широкая semantic platform.

## Как читать этот файл

- Приоритет в имени задачи (`blk/crt/maj/min`) остаётся как локальная важность задачи.
- Приоритеты ниже отвечают на другой вопрос: **что мешает выпуску MVP v1 прямо сейчас**.
- `P0` — блокер MVP v1.
- `P1` — желательно сделать в текущей волне, но первый MVP возможен и без этого.
- `P2` — после первого MVP-тега / `v0.1.x`.
- `OUT` — вне MVP v1; не считать релизным блокером.

## Уже сделано

| Блок | Задачи | Что уже есть |
|------|--------|--------------|
| Diff/core foundation | #018, #023, #024, #030 | `--diff`, fast-path без C/C++ изменений, in-memory diff, baseline save/load |
| Reporting / CI | #025, #028, #055 | PR-comment integration, rules/report core, JSON hygiene |
| Trusted signal hardening | #034, #035, #038, #049, #068, #069 | SF.* фиксы, vendor/vendored noise control |
| Config format contract | `v1_maj_config_format_minimal_contract`, #051 | schema + loader/validation, но **без runtime policy** |

Вывод: **ядро MVP уже существует**. Остаток — это не “написать инструмент с нуля”, а **довести shipped core до честного продуктового состояния**.

## P0 — блокеры MVP v1

| Блок | Задачи | Почему это блокер |
|------|--------|-------------------|
| ~~Контракты и alignment~~ | ~~#073~~, ~~#045~~ ✅ completed 2026-06-12 | выровнены контракты, docs актуализированы |
| ~~Product-grade diff workflow~~ | ~~#075~~ ✅ completed 2026-06-12 | advisory-first + stable JSON output shipped |
| Trust floor для SF.9 | ~~#032~~ ✅ реализовано 04b523b | conditional-рёбра трекаются, SF.9 пропускает all-conditional циклы |
| **Copypaste precision на корпусе** | **#103 wip** | per-commit precision нужна до публикации (MVP.md §Acceptance #10); скрипт готов, нужен overnight прогон 185 реп |
| **new-clone-gate в --diff** | **#123 wip** | ядро закоммичено 344870f: `detectNewClones` + правило DRIFT.NEW_CLONE, advisory в `--diff`, gated bulk-import guard'ом (#117). Остаток: parent-guard (клон, который коммит лишь задел) — «next step» по плану |

## P1 — делать в текущей волне, если не ломает P0

| Блок | Задачи | Комментарий |
|------|--------|-------------|
| Metric drift expansion | ~~#029~~ ✅ реализовано c480e39 | chain length / god-headers / NCCD-дельты в RegressionReport shipped; файл задачи ждёт закрытия |
| Cheap graph signals | #057 | хороший следующий слой ценности после базового diff core |
| Small UX/doc cleanup | #046 | полезно, но не блокирует MVP |
| Security hardening | ~~#105~~ ✅ completed 2026-06-11 | S3–S6 закрыты (cb6e09d): symlink escape, 64 MiB limit, RFC 8259 jsonEscape, git hardening |

### P1-хвост внутри `#073`

- Разрезать `src/main.cpp` на более тонкий application layer.
- Усилить SF.8 до реального include-guard pattern.
- Обновить устаревший `AGENTS.md`.

Эти вещи важны, но **не должны тормозить MVP v1**, если P0 уже закрыт.

## P2 — после первого MVP-тега

| Блок | Задачи / статус | Комментарий |
|------|------------------|-------------|
| Runtime config policy | отдельной задачи пока нет | это уже `v0.2`: `layers` / `forbidden` / `independence` как реальное enforcement |
| SARIF / richer integrations | `future/#005` | adoption-layer, не first-wedge |
| Selective semantic expansion | `future/#042`, `future/#050` | после стабилизации drift core |
| Post-audit cleanup | ~~#104~~ ✅ completed; остаток — `new/#108` | секция 1 закрыта (be56245); секции 2–4 (дубли, lizard-долг) вынесены в #108 |

## OUT — вне MVP v1

### Duplication / clone-detection ветка

- `dropped/#053` (superseded #056), `wip/#054`, `wip/#056` (спайк done, остаток = #072), `wip/#060` (остаток = #070), `wip/#061` (done-but-not-moved), `wip/#070`, `wip/#072`, `wip/#083` (blocked)
- `dropped/#062` (поглощена дизайном #072), `dropped/#064` (absorbed #065/#069/#070), `new/#088` (3/5 пунктов уже в коде, остаток: №2.3 + перепрогон)
- `completed/#063`
- `future/#052`, `future/#071`

Это **не релизные блокеры MVP v1**. Держать отдельно от MVP-борда.

### Cheap-drift / complexity wave (2026-06-10..11, из cheap-guards исследования)

- Реализовано (cb6e09d, 2026-06-11; в wip до переноса в completed): ~~#096~~ (SATD
  delta), ~~#097~~ (test co-evolution), ~~#098~~ (god-file growth + `--history`),
  ~~#100~~ (defect attractor).
- Продуктовые кандидаты, advisory/delta-first: `new/#093` (flag-argument), `new/#094`
  (param accretion), `new/#095` (config-bag), `new/#099` (indentation proxy —
  fallback/absorbed by #101), `new/#101` (local complexity drift).
- Research: `wip/#102` (прототип #101 на корпусе; вердикт `revise`, см.
  `docs/research/local_complexity_drift_scorer_review.md`), `wip/#103`
  (copypaste per-commit — **MVP release-блокер**, overnight прогон 185 реп).

Не блокеры MVP v1; порядок остатка волны: #093 (shared text/signature scan) →
#094/#095/#099/#101; git-ветка (#096→#097/#098/#100) уже закрыта, инфраструктура
`git_exec`/`diff_query` shipped и доступна для реюза.

### AI-repo corpus / methodology / discovery research

- `new/#074` (`#048` и `#067` — completed)
- `wip/#066`, `wip/#079`
- `future/#033`

Это research / corpus-ops трек, а не продуктовый MVP-трек.

### AI/config synthesis и прочий future scope

- `future/#010`
- `future/v1_maj_ai_config_iterative_loop`
- `future/v1_maj_ai_config_synthesis_eval_protocol`
- `future/v1_min_ai_config_synthesis_trial_spdlog`

## Активные WIP, которые не надо путать с MVP

| Задачи | Статус для MVP |
|--------|----------------|
| ~~#041~~ | completed |
| #054, #066 | active research; не считать релизным хвостом MVP v1 |
| #056, #060, #061 | done / делегированы (#072, #070) — кандидаты на закрытие, см. backlog_review.md |

## Рекомендуемый порядок

1. ~~Закрыть `P0` slice из `#073`~~ ✅ completed 2026-06-12.
2. ~~Довести `#075` до product-grade diff workflow~~ ✅ completed 2026-06-12.
3. ~~Ядро `#123`~~ ✅ закоммичено 344870f (advisory new-clone в `--diff`).
4. **Запустить `#103` overnight** (185 реп, ~5–10 ч) → пороги для #123.
5. **Добить `#123`** — parent-guard (клон, который коммит лишь задел).
6. После MVP-тега: `#057`, далее cheap-drift wave. (`#088`/`#116` закрыты.)

## Сжатый вердикт

До MVP v1 осталось **2 активных хвоста**:

1. `#103` wip — overnight copypaste scan (185 реп) + анализ (пороги для #123).
2. `#123` wip — ядро закоммичено (344870f); остаток = parent-guard.

`#073`, `#075`, `#032`, `#045` — все закрыты. Если #103 и #123 закрыты, MVP v1 тегируется.
