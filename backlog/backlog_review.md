# Backlog Review — 2026-05-29 (late)

> Обновление к утреннему ревью. С тех пор закрыто 6 задач (#043, #044, #047, #048→fixture, #049, v1_maj_config_format), три переехали в `future/` после спайка #043, заведено 2 новые задачи (#048 drift methodology, #053 fast-backend line dup), одна стартовала (#051 config loader).

## TL;DR

- `wip/`: **1** (#051 config_loader_v1 — стартовала сегодня).
- `new/`: **7** активных, заблокированных извне — нет.
- **Быстрые победы**: #046 (color TTY decision, ≤2 ч), #048 drift methodology (≤1 ч doc + helper).
- **Средние**: #029, #032, #045.
- **Большая**: #053 (fast-backend line duplication — L, целится в v0.2).
- **Под скоупом / unknown**: #041 (audit hardcoded strings — `Сложность: unknown`, перекрывается с config-format/loader). Подвешено с утреннего ревью, **решение надо принять сегодня-завтра** пока #051 в работе.

**Коллизия номеров:** `new/048_maj_drift_clean_checkout_methodology.md` использует номер, уже занятый `completed/048_maj_fixture_libresprite_pr581.md`. Перенумеровать новую (например в #054).

---

## Закрыто с прошлого ревью (за день)

| Файл | Что закрыло |
|------|-------------|
| #039 SF.8 ObjC exclusion | перенесено в `completed/` ✅ |
| #043 spike clang perf | завершён, `docs/dev/spike_clang_perf.md`. Подтвердил двух-бекендную схему (libclang ×1350 медленнее regex). |
| #044 README sync | done |
| #047 UTF-8 BOM | done |
| #048 LibreSprite PR#581 fixture | done (номер занят) |
| #049 SF.9 include guard false conditional | done |
| `v1_maj_config_format_minimal_contract` | спека `docs/config_format.md` зафиксирована, перешла в completed |

Переехали в `future/` после спайка #043:
- #042 (clang semantic backend) — v0.2
- #050 (SF.21 anon namespace) — v0.3
- #052 (cross-TU duplication AST) — v0.2
- #033 (AI drift dataset) — v0.2+, `Статус: in-progress`

---

## WIP

| Файл | Стартовала | Скоуп |
|------|------------|-------|
| `wip/051_maj_config_loader_v1.md` | 2026-05-29 | YAML→`Config` struct + валидация + fixtures. Loader-only, без подключения к rule pipeline. По git-логу: phase 1 (rules dispatcher) и phase 2 закоммичены (`2893aed`, `574516d`, `f3377ce`). Активная работа. |

---

## Быстрые победы

| Файл | Цель | Модуль | Оценка |
|------|------|--------|--------|
| `new/046_min_docs_color_tty_decision.md` | Track A (isatty + ANSI + NO_COLOR) или B (убрать из роадмапа). Одинаковая стоимость. | DOCS / REPORT | XS (≤2ч) |
| `new/048_maj_drift_clean_checkout_methodology.md` | Доказана причина false positives на DRIFT.1 (dirty checkout). Нужен helper-скрипт `clean_checkout.sh` + параграф в методичке. | RESEARCH | S (≤1ч) |

Обе — без инженерного риска, обе разблокируют дальнейшее: #046 закрывает doc/code drift, #048 делает воспроизводимыми все будущие drift-прогоны.

---

## Средние задачи

| Файл | Цель | Модуль | Сложность |
|------|------|--------|-----------|
| `new/029_maj_metric_regression_detection.md` | chain length / new god-headers / CCD-ACD-NCCD дельты в `RegressionReport` + git-интеграционные тесты. | GRAPH / DIFF | M |
| `new/032_maj_conditional_include_cycles.md` | `#include` под `#ifdef` → `conditional` edge; SF.9 пропускает all-conditional циклы. Снимает 22 FP на spdlog. | SCAN / RULES | M |
| `new/045_maj_docs_sync_roadmap_mvp_spec.md` | MVP.md переписать с нуля, выровнять spec v0.1/v0.2, ADR-001..003. После #044 имеет на что ссылаться. | DOCS | M (1–2 дня) |

---

## Сложные

| Файл | Цель | Сложность | Замечание |
|------|------|-----------|-----------|
| `new/053_maj_fast_backend_line_duplication_pass.md` | Порт line-based Type-1 spike в `src/scan/` как off-by-default проход; cross-component reuse-edge сигнал + ratio. | L | Целится в v0.2, после спайка `experiments/line_duplication/`. Не блокирует ничего в v0.1. Связана с #052 (AST-слой) — два слоя одной темы. |

---

## Без анализа / под вопросом

| Файл | Что не хватает | Рекомендация |
|------|----------------|--------------|
| `new/041_maj_audit_hardcoded_strings.md` | `Сложность: unknown`. Висит с 2026-05-28. Перекрывается с #051 (config_loader_v1) и с `docs/config_format.md`. | Решить **сейчас**, пока #051 в работе: либо доскопить до «фаза 0: grep-таблица + классификация» и втянуть как input в #051 фазу 2, либо переложить в `future/`. Один параллельный поток на config, не два. |

---

## Дубли / связанные

| Файлы | Предложение |
|-------|-------------|
| `new/053` (fast line dup) ↔ `future/052` (AST duplication) | Сознательная пара (cheap layer + precise layer). `Related:` в обе стороны прописан. Не дубль. |
| `new/041` (audit strings) ↔ `wip/051` (config loader) ↔ `docs/config_format.md` | Скрытое пересечение: #041 хочет вынести дефолты в Config, #051 уже это делает по спеке. Линковать `Related:` и/или поглотить #041. |
| `new/045` (docs sync) ↔ `completed/044` (README sync) | #044 закрыт, #045 опирается на него — связь корректна. |

---

## Коллизии и гигиена

- **#048 номер занят дважды.** `completed/048_maj_fixture_libresprite_pr581.md` (закрыта сегодня) и `new/048_maj_drift_clean_checkout_methodology.md`. Перенумеровать новую → **#054** (после #053). Это вопрос гигиены: `Related: #048` в любых других файлах станет двусмысленным.
- **`new/033_maj_ai_drift_dataset.md`** — нет в `new/`, перенесена в `future/` со статусом `in-progress`. Если работа над ней активна — формально это `wip/`, не `future/`. Либо вернуть в `wip/`, либо статус поправить на `paused`/`new`.

---

## Состояние future/ и pending/

- `future/` — 9 задач: 005 SARIF, 010 AI rule synthesis, 033 AI drift dataset, 042 clang backend, 050 SF.21, 052 cross-TU dup, три `v1_*`. Все за горизонтом v0.1.
- `pending/` — не трогаю (правило [[feedback_pending_folder]]).

---

## Сводка

- WIP: **1** (#051).
- Активных в `new/`: **7**.
- Quick win: **2** (#046, #048 drift methodology).
- Средние: **3** (#029, #032, #045).
- Большая: **1** (#053).
- Под вопросом скоуп: **1** (#041).
- Заблокировано внешне: **0**.
- Закрыто с утра: **6** + 4 переезда в `future/`.

## Рекомендуемый порядок ходов

1. **5 мин гигиены** — перенумеровать `new/048` → `#054`; поправить статус `future/033` на `wip/` или `paused`.
2. **Решить #041** — пока #051 в работе, либо втянуть фазу 0, либо в `future/`.
3. **#048 drift methodology** — 1 час, разблокирует будущие drift-прогоны.
4. **#046 color TTY** — 2 часа, закроет doc↔code drift.
5. **Дождаться #051** — потом #029 / #032 / #045 в любом порядке.
6. **#053** — после v0.1 ката, это v0.2-материал.
