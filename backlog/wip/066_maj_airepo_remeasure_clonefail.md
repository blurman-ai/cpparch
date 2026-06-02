# [RESEARCH][SCAN] Докачка part 2: перемер CLONEFAIL + расширение корпуса ≥300

**Дата создания:** 2026-06-01
**Дата старта:** 2026-06-01
**Статус:** wip
**Модуль:** RESEARCH / SCAN
**Приоритет:** major
**Related:** #060 (валидация чекера), #054 (dense-корпус), #065 (generated-skip)

## Цель

Починить массовый CLONEFAIL в measure-фазе и перемерить упавших кандидатов, чтобы
найти ИСТИННОЕ число AI-плотных реп с ≥300 коммитов/год (сейчас занижено ~в 3 раза)
и доклонировать недостающие в `~/oss/_aidev_dense`.

## Контекст — найденный баг (2026-06-01)

`measure_candidates.sh` гонялся с P=14–16 параллельных blobless-клонов. GitHub начал
отбивать соединения → **CLONEFAIL = 16399 из 24642 (66%!)**. Реально измерено лишь
8247. Из них ≥300 коммитов/год = 794, AI-плотных (conc≥5) = 81.

**Следствие:** крупные активные репы клонируются дольше → чаще падали → именно
commit-dense репы систематически терялись. Истинное число ≥300 оценочно ~2400
(794 / 0.33), AI-плотных ≥300 — кратно больше 81.

**Подтверждение (с поправкой):** переклон 20 CLONEFAIL с P=1, timeout 120с → оживают
**6 из 20** (str1ker и др.). Значит CLONEFAIL — СМЕСЬ причин: часть concurrency/rate-limit,
часть таймаут на крупных репах (120с мало для blobless большой истории — напр. FlexEngine),
часть реально мёртвые/private. Undercount-фактор поэтому <3× (TBD при правильном перемере
P=2 + timeout 300 + ретраи). Главное — он значимый, ≥300 занижено.

## План выполнения

- [x] Починить `measure_candidates.sh`: дефолт P=6→3, ретрай-обёртка вокруг clone
      (RETRIES=3 = ретрай ≥2) с нарастающим пейсингом. Источники бага P=16 в
      `keep_downloading.sh` и `discover_finish4.sh` снижены до P=3 (домер) / P=2 (ретрай).
- [~] Перемерить CLONEFAIL с P=2–3. **Сначала сэмпл 2000** (решение пользователя) для
      оценки undercount-фактора без 45ч полного прогона → `sample_estimate.sh` (durable,
      detached), пишет `sample2000_measured.tsv` + funnel в `sample_estimate.log`.
      Sanity 12 репо: ожило 4/12 (~33%), все <300 (skip). Полный перемер — после сэмпла.
- [x] Size-cap при клоне в `_aidev_dense`: 1.5ГБ → **500 МБ** (решение пользователя,
      отсекает 10 тяжёлых реп из 81 dense, ~9.4ГБ). Добавлен API-precheck размера в
      `clone_expand.sh` (не качать заведомо крупные). Потолок диска `_aidev_dense` = 45 ГБ.
- [ ] Пересобрать пул ≥300 и clonelist (conc≥5) по обновлённым данным.
- [ ] Доклонировать новые AI-плотные ≥300 в `_aidev_dense` (per-repo cap 500 МБ, диск 45 ГБ).
- [ ] harvest2 (новая ось: `sort=stars`, окно pushed назад). C-проход НЕ включать.
- [ ] Обновить EXPANSION_REPORT + METHODOLOGY с честным funnel (с поправкой на баг).

### Замер dense-корпуса (2026-06-01, агент через `gh api`)
81 AI-плотных ≥300 (conc≥5), все живые, все C++, ни одной archived. Суммарно ~16.5 ГБ.
Тяжёлых >500 МБ — 10 (HyperXTalk 2.6ГБ, arduino-esp32 2.1ГБ, UE5-MCP 1.4ГБ, Fincept 769,
Exasim 767, piper-plus 728, NAAb 716, LogViewer 617, gse_fork 583, wiRedPanda 565) =
~9.4 ГБ / 56% бюджета. Без них 71 репо ≈ 7.5 ГБ. Топ по коммитам/год: deltahdl (9911),
kiln (4444), PlasmaZones (3757), komai (3419), NereusSDR (2788).

## Результат оценки (сэмпл 410 случайных CLONEFAIL, 2026-06-01)

Баг подтверждён, занижение **~2.5–3×**:

| метрика | было | +из перемера (экстрап.) | истинное |
|---|---:|---:|---:|
| revival CLONEFAIL | — | **89%** клонируются | баг = почти весь rate-limit |
| ≥300 коммитов/год | 794 | +~1670 | **~2460** (×3.1) |
| AI-плотных conc≥5 ≥300 | 81 | +~120 | **~200** (×2.5) |

Сэмпл влит в MEAS → CLONEFAIL 16399 → **16028**. Найденные dense в перемере: Banana (conc 81),
QCView-Player (13), esphome/esphome (6639 комм/год, conc 9).

## Reboot-safety (2026-06-02): возобновление после перезагрузки в Windows

Всё состояние вынесено из `/tmp` (очищается при reboot) в постоянный
`~/oss/_aidev_state/` (на ext4, переживает reboot в Windows).
Скрипты сделаны resumable (пропускают сделанное), автозапуск через `@reboot`.

| Процесс | Resumable-механизм | Файл |
|---|---|---|
| Перемер CLONEFAIL | OUT `_aidev_state/clonefail_remeasured.tsv`, skip по repo, merge+флаг `remeasure.done` | `remeasure_resumable.sh` |
| Докачка dense | ждёт `remeasure.done`, `clone_expand` идемпотентен, флаг `finish.done` | `finish_resumable.sh` |
| round2 верификация | пропуск реп с готовым `verify_results2/<repo>.json`, exclude из стейта | `verify_findings_round2.js` (resume-фильтр) |
| Автозапуск @reboot | `/etc/cron.d/airepo_resume` → master, идемпотентен (флаги+pgrep) | `resume_all.sh`, `round2_headless.sh` |

При следующей загрузке Linux cron сам поднимет `resume_all.sh` → перемер+финишер
(shell) + round2 (headless claude) продолжатся с точки останова. Ручной запуск:
`bash experiments/ai_repo_run/resume_all.sh`.

## Стратегия «не качать лишнего» (2026-06-02)

Корень неэффективности: клон тянул весь граф истории, а меряем только пост-май.
+ непойманные гиганты (chromium, o3de) застревали на таймаут-ретраях.

| Лекало | Реализация |
|---|---|
| **shallow-since=2025-05-01** (главный) | `measure_candidates.sh`: тянем только пост-май историю → cut в разы; общий предохранитель против любых гигантов |
| name-фильтр зеркал | `remeasure_resumable.sh` GIANT-regex: chromium/llvm/aosp/webkit/gecko/o3de/unreal/tensorflow/src-leak → TOOBIG-skip без клона |
| API-фолбэк на падении shallow | 1 вызов `commits?since` различает «нет свежих» (skip) vs CLONEFAIL |

Граница: conc нельзя узнать без измерения → 16k всё равно проходим, но дёшево.

## В работе (durable, resumable — снапшот 2026-06-02 ~14:4x)

- **Перемер 16028** (resumable, пишет в стейт): OUT **7616**, новых dense **62**, TOOBIG-skip 10.
  Процессы вычищены вручную (зомби-`xargs` залипали на o3de; kill из sandbox не доходит —
  пользователь бил из терминала). На resume поднимется один чистый инстанс с shallow-since.
- **round2 верификация**: **66/135** реп.
- **Финишер dense** ждёт `remeasure.done`. Корпус **29 ГБ / 367 дир**. Scratch-мусор ~3.7ГБ (чистится).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| P=2–3 вместо 14–16 | P=16 → GitHub rate-limit → 66% CLONEFAIL |
| ретрай + пейсинг | разовый клон ненадёжен под нагрузкой |
| size-cap 500 МБ (было 1.5ГБ) | 10/81 тяжёлых = 56% диск-бюджета; широта важнее |
| API-precheck `.size` перед клоном | не качать заведомо крупные репы (а потом удалять) |
| сорт клонлиста по коммитам/год | максимум авторского сигнала; `commits/МБ` — вторичная метрика плотности |
| `language:c` убран | это чистый C, не C++ — мусор для исследования |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `measure_candidates.sh` | дефолт P 6→3, ретрай (RETRIES=3) + пейсинг, **shallow-since=2025-05-01**, API-фолбэк |
| `remeasure_resumable.sh` | + GIANT name-фильтр зеркал (chromium/o3de/llvm/…) → TOOBIG-skip |
| `clone_expand.sh` | cap 600→500, API-precheck размера перед клоном |
| `keep_downloading.sh` | measure P=16→3, clone cap 1500→500 |
| `discover_finish4.sh` | measure P=16→3 (ретрай P=2), clone cap 1500→500 |
| `remeasure_clonefail.sh` (new) | перемер CLONEFAIL P=2 + merge/дедуп в MEAS |
| `sample_estimate.sh` (new) | оценка undercount на сэмпле 2000 |
| `remeasure_resumable.sh` (new) | resumable-перемер (стейт, skip-done, флаг) |
| `finish_resumable.sh` (new) | resumable-докачка dense по флагу `remeasure.done` |
| `resume_all.sh` + `round2_headless.sh` (new) | мастер @reboot-возобновления + headless round2 |
| `verify_findings_round2.js` (new) | round2: копипаст 3× (exclude round1) + cycle-intro археология, resume-фильтр |
| `round2_resume_prompt.txt` (new) | промпт headless-возобновления round2 |
| `new300_measured.tsv` | дозапись перемеренных (через merge resumable) |
| `harvest2.sh` | новая ось (cpp-only) — план |

Стейт (не в git, persistent): `~/oss/_aidev_state/`. Cron: `/etc/cron.d/airepo_resume`.

## Примечание
Разнесено из сессии 2026-06-01: текущий фокус — АНАЛИЗ имеющегося корпуса
(`CORPUS_CHECK_REPORT.md`, 112 ≥300-реп), докачка вынесена сюда (part 2).
