# Backlog Task Tracker — MVP v1

_2026-06-02_

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
| Trust floor для SF.9 | #032 | известный реальный FP-класс на conditional include циклах подрывает доверие к default signal |

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
| Metric drift expansion | #029 | усиливает diff/report, но не обязателен для первого trusted wedge |
| Cheap graph signals | #057 | хороший следующий слой ценности после базового diff core |
| Small UX/doc cleanup | #046 | полезно, но не блокирует MVP |

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

## OUT — вне MVP v1

### Duplication / clone-detection ветка

- `wip/#053`, `wip/#054`, `wip/#056`, `wip/#060`, `wip/#061`, `wip/#072`
- `new/#062`, `new/#063`, `new/#064`, `new/#070`
- `future/#052`, `future/#071`

Это **не релизные блокеры MVP v1**. Держать отдельно от MVP-борда.

### AI-repo corpus / methodology / discovery research

- `new/#048`, `new/#067`, `new/#074`
- `wip/#066`
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
| #041 | не MVP blocker |
| #053, #054, #056, #060, #061, #066, #072 | active research / preview; не считать релизным хвостом MVP v1 |

## Рекомендуемый порядок

1. Закрыть `P0` slice из `#073` вместе с `#045`.
2. Закрыть `#032`, чтобы default SF.9 перестал ловить известный conditional-FP класс.
3. Довести `#075` до product-grade diff workflow.
4. После этого добирать `#029` и `#057`.
5. Всё duplication / AI-research считать отдельной дорожкой, не смешивать с MVP.

## Сжатый вердикт

До MVP v1 осталось **3 продуктовых блока**, а не весь текущий backlog:

1. `#073 + #045` — выровнять контракты и документы.
2. `#032` — убрать известный громкий FP в trusted signal.
3. `#075` — сделать один понятный product-grade diff workflow.

Если эти три блока закрыты, MVP v1 можно считать собранным.
