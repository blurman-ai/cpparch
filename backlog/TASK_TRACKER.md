# Backlog Task Tracker — MVP v1

_2026-06-23 (пятый проход); этот файл — SSOT по release-readiness v0.1. Core MVP
уже реализован: #103 дал product precision, #123 shipped как advisory `--diff`
с parent-guard и durable local/Catch2 tests. Оставшееся делится на board hygiene
(формально закрыть/перенести #103) и public-readiness перед анонсом (#123 GitHub
test repo, #127/#131 vendored/generated applicability sign-off)._

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
| ~~Copypaste precision на корпусе~~ | ~~#103 wip → close~~ | ✅ product precision получена: 70-findings triage, ≈86–91% precision; full 185-repo run не нужен для MVP. Остаток = board move/cleanup, не dev blocker |
| ~~new-clone в --diff~~ | ~~#123 wip~~ | ✅ shipped advisory: `DRIFT.NEW_CLONE`, parent-guard, local 10/10 control set, Catch2 E2E. Остаток = outward-facing GitHub test repo для публичной демонстрации, не core MVP blocker |

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
  (copypaste per-commit — product precision получена; остался board cleanup).

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
4. ~~Получить product precision по `#103`~~ ✅ ≈86–91%, full 185-repo run отменён как не несущий решения.
5. ~~Добить `#123` parent-guard + durable tests~~ ✅ local 10/10 + Catch2 E2E.
6. Перед публичным анонсом: #123 GitHub test repo, #127/#131 applicability sign-off.
7. После MVP-тега: `#057`, далее cheap-drift wave. (`#088`/`#116` закрыты.)

## Сжатый вердикт (обновлено 2026-06-19, 5-й проход)

**Core MVP-v1:** code-level blockers не осталось. `#073`, `#075`, `#032`, `#045`,
`#105`, `#029`, `#103` product precision и `#123` product path закрыты по сути.
Нужна board hygiene: перенести/закрыть #103 и решить, оставлять ли #123 в wip ради
GitHub test repo или вынести этот демо-хвост отдельной задачей.

**До публичного анонса:**
- `#127/#131` vendored/generated exclusion и corpus sign-off — главный применимостный хвост.
  Без него archcheck на bundled-deps всё ещё может утонуть в vendor/generated шуме
  (supercollider-класс кейсов уже резко улучшен, но bpftrace/newsboat хвосты открыты).
- `#123` GitHub test repo — outward-facing validation/demo, не core behavior.
- First-run sanity на 3–5 известных репах.

**Закрыто этой сессией:** #128 (SF.8 баннер), #130 (findFile), #108 (hardening SSOT).
**Гигиена борда:** #129 ядро landed (wip, остаток = corpus golden #131 → near-close);
#122 grow+measure done (→ completed); #119 разблокирован; #072 — одно scope-решение (dup-pairs JSON).
