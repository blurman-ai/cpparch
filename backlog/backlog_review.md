# Backlog review — 2026-06-05

Snapshot активной очереди. Источник: `new/` (16), `wip/` (13), `future/` (11) = **40 активных**, кросс-сверка с `completed/` (43). `pending/` не трогался.

## Сводка

- **Протухших по дате нет.** Самый старый активный файл — 2026-05-28 (8 дней). Порог 30 дней не сработал ни на одном.
- **3 задачи done-but-not-moved** сидят в `wip/`: #080, #081, #072_min — готовы, ждут только `git mv → completed/` (это работа `/fix-issue`, не этого скила).
- **2 задачи mostly_done** (один шаг до закрытия): #041 (4/5 шагов), #061 (фикс применён, остался 1 чекбокс верификации).
- **1 задача — отрицательный результат с неверным статусом**: #063 закрыта как тупик и передана в #070, но помечена «new».
- **1 misfiled**: `new/071_fp_classification_rules.md` сам себя помечает `Статус: wip, Дата старта 2026-06-03` — физически лежит в `new/`.
- **3 коллизии номеров**: 048, 071, 072 (детали ниже).
- **Разблокирован кластер**: `v1_maj_config_format_minimal_contract` уже в `completed/` → корневой блокер AI-config-synthesis снят (#010 тоже разблокирован — его блокер #008 завершён).
- **2 stub-задачи без анализа**: #062, #064.

---

## Протухшие / требуют смены статуса

Классической «протухи по дате» нет. Ниже — задачи, чей **статус не отражает реальность** (результат уже сделан / задача закрыта иначе):

| Файл | Причина | Рекомендация |
|------|---------|--------------|
| `wip/080_manual_corpus_analysis_findings.md` | В шапке уже `Статус: completed`, «✅ Выполнено полностью», коммиты f29429a + b93729a | **Move to completed** (`/fix-issue`) |
| `wip/081_maj_dup_scanner_misses_overexclusion.md` | Фикс влит — коммит f68035a в git log, 344 теста зелёные, все чекбоксы | **Move to completed** |
| `wip/072_min_clone_type_classifier.md` | LD.10+LD.11 реализованы и проверены на фикстуре, «Следующие шаги: git mv wip → completed» | **Move to completed** |
| `new/063_crt_diff_coincidental_precision.md` | Зафиксирован отрицательный результат (idiom-FP floor), поверхностный путь закрыт, решение передано в #070 | **Move to completed** как resolved/transferred — не actionable сам по себе |
| `new/071_fp_classification_rules.md` | Сам себя помечает `wip` + `Дата старта 2026-06-03`, есть uncommitted изменения в src/tests | **Keep, но переместить new → wip** (рассинхрон папки и статуса) |
| `wip/041_maj_audit_hardcoded_strings.md` | 4 из 5 шагов сделаны (phase-1 loader = completed/051); остался шаг 5 (kProjectExtensions/kHeaderExtensions) + мелкий дедуп | **Keep in WIP** — дожать шаг 5, ~quick_win |
| `wip/061_maj_diff_rename_path_validation.md` | Фикс `-M` применён (main.cpp ~839, пересобран); остался 1 чекбокс верификации (iter2) | **Keep in WIP** — закрыть верификацией, затем completed |

---

## Быстрые победы (actionable сейчас, < 1 дня)

| Файл | Цель | Модуль |
|------|------|--------|
| `new/046_min_docs_color_tty_decision.md` | isatty+ANSI color в text reporter **или** убрать обещание из spec (XS, ≤2ч) | REPORT |
| `new/048_maj_drift_clean_checkout_methodology.md` | Зафиксировать clean-checkout методологию + helper-скрипт (S, ≤1ч; скрипт уже написан в теле задачи — осталось положить в `scripts/`) | FIXTURES |
| `wip/061_maj_diff_rename_path_validation.md` | Дожать верификацию rename-фикса (фикс уже в коде) | SCAN |
| `wip/041_maj_audit_hardcoded_strings.md` | Шаг 5 — расширения файлов в Config | CONFIG |

> #064, #050, #072_min, #080, #081 формально «быстрые», но: #064 — stub без дизайна (см. ниже), #050 заблокирован #042, остальные три — уже done (см. протухшие).

---

## Средние задачи

| Файл | Цель | Модуль | Сложность |
|------|------|--------|-----------|
| `new/029_maj_metric_regression_detection.md` | Метрические регрессии (chain/god-headers/CCD) в RegressionReport | GRAPH | medium |
| `new/032_maj_conditional_include_cycles.md` | Помечать `#ifdef`-include как conditional → убрать SF.9 FP-циклы (22 FP на spdlog) | SCAN | medium |
| `new/045_maj_docs_sync_roadmap_mvp_spec.md` | Синхронизировать MVP/spec/ADR с фактическим v0.1/v0.2 скоупом | DOCS | medium |
| `new/057_maj_lakos_fanout_coupling_checks.md` | Правило Lakos.GodComponentFanOut + edges/nodes/blast-radius метрики | RULES | medium (скрытая калибровка порога) |
| `new/074_maj_ai_repo_discovery_roi_alignment.md` | Свести ROI/giant-skip между основным и resumable discovery-путём (experiments/) | SCAN | medium |
| `new/077_maj_per_commit_graph_drift_export.md` | Скрипт per-commit экспорта include-графа (experiments/, research-only) | GRAPH | medium |
| `new/071_fp_classification_rules.md` (статус wip) | Зафиксировать объективные TP/FP-правила дубликатов | SCAN | medium |
| `future/010_maj_ai_rule_synthesis_contract.md` | Контракт AI-assisted rule synthesis (блокер #008 — снят) | RULES | medium |
| `future/033_maj_ai_drift_dataset.md` | C++ репо с AI-коммитами как fixtures для DRIFT.1/2 (активно движется → кандидат в wip) | FIXTURES | medium |
| `future/v1_maj_agent_config_authoring_rules.md` | Правила для агента, заполняющего `.archcheck.yml.draft` (блокер снят) | DOCS | medium |
| `future/v1_maj_ai_config_iterative_loop.md` | Контракт итеративного цикла синтеза конфига | CONFIG | medium |
| `future/v1_maj_ai_config_synthesis_eval_protocol.md` | Протокол Claude vs Codex для генерации конфига | DOCS | medium |
| `future/005_maj_sarif_reporter_spec.md` | Маппинг violations → SARIF 2.1.0 (spec-only, self-contained) | REPORT | quick→medium |

---

## Сложные / заблокированные

| Файл | Блокер |
|------|--------|
| `new/073_maj_tech_debt_alignment_cleanup.md` | Umbrella-долг (10 пунктов); сам **блокирует** #075. Зависит от #045. Самая крупная в наборе (L) |
| `new/075_crt_mvp_v1_trusted_diff_workflow.md` | Заблокирован P0-срезом #073 + #045. Critical, но порядок важен |
| `new/067_maj_overnight_eye_verification.md` | Ночной cron-верификатор 138 реп; зависит от #060/#056/#054 + актуального CORPUS_CHECK_REPORT |
| `new/078_maj_clone_cochange_harm_signal.md` | Co-change severity; ждёт стабильной базы пар (#056/#070/#071) + git-history infra (#054) |
| `wip/053_maj_fast_backend_line_duplication_pass.md` | Спайк закрыт, порт в src/ не начат; **блокирует** #056. P0-B/C открыты |
| `wip/056_maj_fast_backend_partial_duplication_pass.md` | Заблокирован #053 (plumbing). Спайк готов |
| `wip/060_maj_checker_validation_hardening_loop.md` | iter1–4 + corpus round2 сделаны; ждёт встройки confirm-слоя #070 как гейта |
| `wip/066_maj_airepo_remeasure_clonefail.md` | Живой фоновый перемер (7616/16028, round2 66/135). Resumable, ждёт догрузки |
| `wip/070_maj_checker_fp_fix_proposals.md` | P0 (6 гардов) влиты; P1 отложен из MVP. Файл 288KB — содержит сырой корпус |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | Phase 1+2 done; остаток: FP-гарды (#070), reporters, fixtures, dogfood |
| `wip/079_maj_corpus_run_graph_dup_ai_correlation.md` | Корпус 513 реп сделан; активный фоновый find_revived.py (≥1000 реп H2-2025) |
| `future/042_maj_clang_semantic_backend.md` | Правильно в future/v0.2; **блокирует** #050, #052. Спайк #043 ✅ подтвердил архитектуру |
| `future/050_min_sf21_anonymous_namespace.md` | Тривиален (~30 строк), но заблокирован #042. Закрывать как #042c |
| `future/052_maj_cross_tu_duplication_detector.md` | Заблокирован #042; Stage 0 perf-gate критичен |
| `future/v1_min_ai_config_synthesis_trial_spdlog.md` | Leaf AI-synthesis; ждёт authoring_rules + eval_protocol |

---

## Без анализа (нужно исследование)

| Файл | Что не хватает |
|------|----------------|
| `new/062_maj_diff_fragment_boundary_align.md` | Stub: только цель (FP-segmentation ~15%) + одна идея. Нет плана, файлов, критерия приёмки. Сложность `unknown` |
| `new/064_min_diff_vendor_exclude.md` | Stub: цель + один чекбокс, без дизайна. **Сначала проверить** — не покрыт ли уже path-guards из #069/#071 (P0.9 isGeneratedPath + third_party) |

---

## Дубли / связанные

| Файлы | Предложение |
|-------|-------------|
| #029 ↔ #057 | Оба трогают `computeGraphMetrics` / fan-in / GraphMetrics. #057 реализует, #029 потребляет. Прописать `Related:` и порядок (#057 → #029), чтобы не делать дважды |
| #053 / #056 / #072_maj / #052 | Комплементарные слои дедупа (line / token / port / AST). **Не дубли** — но границы держать явными (док `duplication_architecture.md` уже это фиксирует) |
| #010 + 4×`v1_*` (authoring/iterative/eval/spdlog) | Кластер AI-config-synthesis. Ортогонален, дублей нет. **Корневой блокер снят** (`config_format_minimal_contract` в completed) → кластер можно двигать |
| #074 ↔ #066 | #074 чинит то, что #066 задокументировал как сделанное, но не внедрил. Уточнить границу |
| #077 ↔ #076 (completed) | #077 даёт инструмент для валидации cross-area drift из #076. Прописать `Related:` |
| #045 ↔ #044 (completed) | Оба про docs-sync; риск дивергенции формулировок. Координировать |
| `new/071_fp_classification_rules` ↔ memory `fp_classification_rules` | Задача — источник memory-записи, уже синхронизированы. Не конфликт |
| #071 (future, clone catalog) ↔ #052/#053/#056/#070 | Umbrella над всем duplication-кластером. LD.10/11/14/16 уже done. Реально новые — только LD.15 (git-blame), LD.17 (hotspots) |

---

## Гигиена нумерации (для владельца бэклога)

Коллизии номеров — `/create-task` присвоил один номер дважды:

| Номер | Конфликт | Заметка |
|-------|----------|---------|
| **048** | `new/048_maj_drift_clean_checkout_methodology` ↔ `completed/048_maj_fixture_libresprite_pr581` | Разные задачи, один номер |
| **071** | `new/071_fp_classification_rules` ↔ `future/071_min_clone_detection_opportunities` | Разные задачи, один номер |
| **072** | `wip/072_maj_port_056_…` ↔ `wip/072_min_clone_type_classifier` | Обе живые в wip |

> `008a–008h` — намеренная буквенная нарезка одной задачи, **не** коллизия.

---

## TASK_TRACKER.md

Существует (`backlog/TASK_TRACKER.md`, дата 2026-06-02) — кураторский P0/P1/P2/OUT-срез под MVP v1, не простой чек-лист. **Этим скилом не правил** (свежий стратегический артефакт). Владельцу стоит отразить в нём done-but-not-moved (#080/#081/#072_min) при следующем апдейте.
