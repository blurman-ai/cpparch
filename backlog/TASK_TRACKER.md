# Backlog Task Tracker — MVP v1

_2026-06-02; ссылки на папки задач синхронизированы 2026-06-11 (точечно, без пересмотра приоритетов). Переносы #079/#096/#097/#098/#100 → completed состоялись. Известный долг трекера: дубли номеров #071 (completed + future), #072 (wip + completed); #029 и #032 реализованы в коде (c480e39, 04b523b), но файлы лежат в new/ — детали в backlog_review.md (второй проход 2026-06-11)._

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
| Контракты и alignment | #073, #045 | сейчас есть ложные user-facing promises и конфликтующие документы |
| Product-grade diff workflow | #075 | pieces уже есть, но нет одной канонической product-story и release-контракта |
| Trust floor для SF.9 | ~~#032~~ ✅ реализовано 04b523b | conditional-рёбра трекаются, SF.9 пропускает all-conditional циклы; остаток — фикстура + spdlog-перепрогон при закрытии файла задачи |

### P0.1 — что именно должно приехать из `#073`

- `--config` для MVP v1 должен стать **честным**: либо validate-only surface, либо runtime-фича.
- Для MVP v1 рекомендованный путь: **validate-only + честные docs/help**, потому что runtime policy уже отнесён в `v0.2`.
- Нужно свести thresholds `check` и `--diff` к одному источнику истины.
- Discovery `.archcheck.yml` должен зависеть от проверяемого root, а не от `cwd`.
- Нужно выбрать один diagnostics contract и выровнять код с документами:
  `file[:line]` для MVP v1 или реальное `file:line:column` с расширением модели.

### P0.2 — что именно должно приехать из `#075`

- Один канонический PR/diff workflow.
- Advisory-first default.
- Явно описанные gated conditions.
- Стабильный `text/json` output.
- Один copy-paste CI example.
- E2E / golden coverage именно для продуктового diff пути.

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
  `docs/research/local_complexity_drift_scorer_review.md`), `new/#103`
  (copypaste per-commit — прекурсор new-clone-gate).

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
| #054, #066, #109 | active research; не считать релизным хвостом MVP v1 |
| #056, #060, #061 | done / делегированы (#072, #070) — кандидаты на закрытие, см. backlog_review.md |

## Рекомендуемый порядок

1. Закрыть `P0` slice из `#073` (`#045` уже completed).
2. ~~Закрыть `#032`~~ — реализовано (04b523b); осталось закрыть файл задачи.
3. Довести `#075` до product-grade diff workflow.
4. После этого добирать `#057` (`#029` реализован, c480e39).
5. Всё duplication / AI-research считать отдельной дорожкой, не смешивать с MVP.

## Сжатый вердикт

До MVP v1 осталось **2 продуктовых блока**, а не весь текущий backlog:

1. `#073` — выровнять контракты и документы (`#045` completed).
2. `#075` — сделать один понятный product-grade diff workflow.

(`#032` — третий бывший блок — реализован 04b523b.) Если эти два блока закрыты, MVP v1 можно считать собранным.
