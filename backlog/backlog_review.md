# Backlog review — 2026-06-11

Snapshot активной очереди. Источник: `new/` (21), `wip/` (18), `future/` (12) = **51 активная**, кросс-сверка с `completed/` (71). `pending/` не трогался. Предыдущий снапшот: 2026-06-05.

## Сводка

- **Протухших по дате нет.** Самый старый активный файл — 2026-05-28 (14 дней). Порог 30 дней не сработал.
- **6 задач done-but-not-moved**: #096, #097, #098, #100 (все реализованы коммитом cb6e09d, сами пишут «готово/закрыто»), #061 (фикс `-M` влит, итерация 2 `0beb2f7` прошла), #067 (отчёты `VERIFICATION_REPORT{,_R2}.md` существуют, результаты уже потреблены #060/#066/#070). Перенос — работа `/fix-issue`, не этого скила.
- **1 ложный статус**: `future/090` — секция «Сделано» утверждает реализацию `src/rules/implicit_state_machine_growth.{h,cpp}` + тесты; **проверено: этих файлов в репо нет**. Артефакт галлюцинации — секцию вычистить.
- **1 misfiled (повтор с 2026-06-05)**: `new/071_fp_classification_rules.md` внутри помечен `wip` + дата старта 2026-06-03, физически лежит в `new/`.
- **3 коллизии номеров** (без изменений): 048, 071, 072.
- **Разблокированы**: `future/v1_maj_agent_config_authoring_rules` (блокер `config_format_minimal_contract` в completed), `future/010` (блокер #008 завершён), `future/033` (блокер #009 снят, статус внутри `in-progress` — кандидат в `wip/`).
- **2 stub-задачи без анализа** (без движения с прошлого ревью): #062, #064.
- Из прошлого ревью **закрыто корректно**: #080, #081, #072_min, #063 переехали в completed; #105 завершена сегодня.

---

## Протухшие / требуют смены статуса

Классической протухи по дате нет. Ниже — задачи, чей статус не отражает реальность:

| Файл | Причина | Рекомендация |
|------|---------|--------------|
| `wip/096_maj_satd_delta.md` | Реализовано в cb6e09d (SATD.1/2, 22 unit + integ, fixtures), «Следующие шаги: готово к ревью и мерджу» | **Move to completed** (`/fix-issue`) |
| `wip/097_maj_test_co_evolution.md` | Реализовано в cb6e09d (TEST.1, 9 unit + integ, fixtures, dogfood чист) | **Move to completed** |
| `wip/098_min_god_file_growth.md` | Реализовано в cb6e09d (`--history`, SIZE.1, e2e); секция «Следующие шаги» — устаревший план до реализации | **Move to completed**, при переносе вычистить устаревшие шаги |
| `wip/100_min_defect_attractor.md` | Реализовано в cb6e09d (HIST.1, 11 unit); сам файл пишет «задача закрыта» | **Move to completed** |
| `wip/061_maj_diff_rename_path_validation.md` | Фикс `-M` влит, верификация в iter2 прошла (коммит `0beb2f7` упомянут в #060 как закрывающий) | **Move to completed** |
| `new/067_maj_overnight_eye_verification.md` | Прогон выполнен: `experiments/ai_repo_run/VERIFICATION_REPORT.md` + `_R2.md` существуют, precision-результаты (32%→36%) уже потреблены #060/#066/#070 | **Move to completed**, дописать итоговые секции |
| `future/090_min_boolean_state_drift_metric.md` | Секция «Сделано» ложная: `implicit_state_machine_growth.{h,cpp}` в репо **нет** (проверено grep по src/include/tests). Нижняя половина файла — чужеродный детальный план реализации, противоречащий верхней («deferred, YAGNI») | **Keep in future**, но вычистить ложное «Сделано» и Haiku-план; оставить честный deferred по итогам #089 |
| `new/071_fp_classification_rules.md` | Внутри `Статус: wip`, дата старта 2026-06-03; лежит в `new/` (повтор с прошлого ревью, не разрулено) | **Переместить new → wip** |
| `wip/060_maj_checker_validation_hardening_loop.md` | Iter 1–4 + корпус round2 сделаны; вся оставшаяся работа делегирована #070 | **Keep in WIP**, закрывать by-proxy после #070 |
| `wip/079_maj_corpus_run_graph_dup_ai_correlation.md` | Секция «Итоги» — «полностью завершено», но «В работе» держит revived-выборку и cajeta | **Keep in WIP**; либо отрезать хвост в отдельную задачу и закрыть |

---

## Быстрые победы (actionable сейчас, < 1 дня)

| Файл | Цель | Модуль |
|------|------|--------|
| `new/046_min_docs_color_tty_decision.md` | isatty+ANSI color в text reporter **или** убрать обещание из spec (≤2ч) | REPORT |
| `new/048_maj_drift_clean_checkout_methodology.md` | Зафиксировать clean-checkout методологию; скрипт уже написан в теле задачи — положить в `scripts/` (≤1ч) | RESEARCH |
| `wip/041_maj_audit_hardcoded_strings.md` | Шаг 5: `kProjectExtensions`/`kHeaderExtensions` в `Config` + README/`--help` | CONFIG |
| `wip/045_maj_docs_sync_roadmap_mvp_spec.md` | Финальная вычитка roadmap-секции спека (Issue 9) — один проход до v0.1-тега | DOCS |
| `new/071_fp_classification_rules.md` | Дооформить правила в `docs/duplication_architecture.md`, закоммитить висящие изменения | SCAN |

> #096/#097/#098/#100/#061/#067 формально «быстрые», но это перенос в completed (см. выше), не работа.

---

## Средние задачи

| Файл | Цель | Модуль | Сложность |
|------|------|--------|-----------|
| `new/029_maj_metric_regression_detection.md` | Метрические регрессии (chain/god-headers/CCD) в RegressionReport | GRAPH | medium |
| `new/032_maj_conditional_include_cycles.md` | `#ifdef`-include как conditional → убрать SF.9 FP-циклы (22 FP на spdlog подтверждены) | SCAN | medium |
| `new/057_maj_lakos_fanout_coupling_checks.md` | Lakos.GodComponentFanOut + edges/nodes/blast-radius в отчёт | GRAPH | medium |
| `new/074_maj_ai_repo_discovery_roi_alignment.md` | Свести ROI/giant-skip/resumable discovery к одному контракту (experiments/) | RESEARCH | medium |
| `new/075_crt_mvp_v1_trusted_diff_workflow.md` | Канонический product-grade diff workflow (critical, ждёт P0-среза #073) | CLI | medium |
| `new/077_maj_per_commit_graph_drift_export.md` | Per-commit экспорт include-графа для ручного eyeball (experiments/) | RESEARCH | medium |
| `new/088_maj_archcheck_fp_from_corpus.md` | 4 класса FP с 767-репного корпуса (angle-resolve, .inl, sccs vs SF.9, vendoring) | RULES | medium |
| `new/093_maj_flag_argument.md` | ARG.1/ARG.2 flag-argument heuristics; база (shared scanner) для #094/#095 | SCAN | medium |
| `new/094_maj_param_count_and_accretion.md` | ARG.3/ARG.4 параметры и accretion (ждёт scanner из #093) | SCAN | medium |
| `new/095_maj_config_bag_growth.md` | Config-bag рост полей (ждёт `text_scan` из #093) | SCAN | medium |
| `wip/054_maj_ai_repo_duplication_run.md` | Достроить корпусный прогон: shortlist 6 реп, within-repo тест | RESEARCH | medium |
| `wip/066_maj_airepo_remeasure_clonefail.md` | Перемер CLONEFAIL: пересборка пула ≥300, доклон, harvest2 | RESEARCH | medium |
| `wip/070_maj_checker_fp_fix_proposals.md` | Measurement-harness + замер эффекта P0-гардов на корпусе | SCAN | medium |
| `wip/104_min_post_audit_cleanup_sweep.md` | Секции 2–4: дубли fork/exec/toLower/JSON, гигиена scanner, lizard-долг | SCAN | medium |
| `future/005_maj_sarif_reporter_spec.md` | SARIF-маппинг; первый шаг — разрешить конфликт фазы (v0.1/v0.2/v0.3 в трёх местах) | REPORT | medium |
| `future/010_maj_ai_rule_synthesis_contract.md` | Контракт AI rule synthesis (разблокирован — #008 done) | RULES | medium |
| `future/033_maj_ai_drift_dataset.md` | AI-репо как drift-fixtures; шаги 1–3 сделаны, статус внутри `in-progress` → кандидат в wip | FIXTURES | medium |
| `future/v1_maj_agent_config_authoring_rules.md` | Правила агента для `.archcheck.yml.draft` (**разблокирована**, черновик почти написан в теле) | CONFIG | medium |
| `future/v1_maj_ai_config_iterative_loop.md` | Контракт итеративного синтеза конфига (ждёт authoring_rules) | CONFIG | medium |
| `future/v1_maj_ai_config_synthesis_eval_protocol.md` | Протокол Claude vs Codex (ждёт authoring_rules) | DOCS | medium |

---

## Сложные / заблокированные

| Файл | Блокер |
|------|--------|
| `new/073_maj_tech_debt_alignment_cleanup.md` | Umbrella 10 пунктов, сам блокирует #075; крупнейшая продуктовая задача. Пункт 7 (docs) частично закрыт #044/#082/#085 — сверить перед стартом |
| `new/078_maj_clone_cochange_harm_signal.md` | Ждёт стабильной базы пар (#056) + git-history infra |
| `new/101_maj_local_complexity_drift.md` | Самая проработанная спека волны; ждёт вердикта прототипа #102 (сейчас `revise`) |
| `new/103_maj_copypaste_per_commit_drift.md` | Research-прекурсор new-clone-gate; скорость клон-матчинга на 185 репах неизвестна |
| `wip/053_maj_fast_backend_line_duplication_pass.md` | Спайк закрыт, порт P0-B/C не начат; блокирует #056 | 
| `wip/056_maj_fast_backend_partial_duplication_pass.md` | Заблокирован #053 (plumbing) |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | Фазы 1–2 done; остаток: FP-гарды #070, reporters, fixtures — hard |
| `wip/083_maj_duplication_precision_gate_grade.md` | **Blocked** на v0.2 semantic backend (P1.3 token-level опровергнут данными). Корректно держится в wip как blocked |
| `wip/102_maj_local_complexity_drift_corpus_prototype.md` | Scorer v2 по итогам ревью (5 дефектов v1), перегон корпуса, обновление вердикта для #101 |
| `future/042_maj_clang_semantic_backend.md` | Разблокирован (#043 done), ждёт старта v0.2; блокирует #050, #052, SF.21-волну |
| `future/050_min_sf21_anonymous_namespace.md` | Тривиален (~30 строк), но заблокирован #042 |
| `future/052_maj_cross_tu_duplication_detector.md` | Заблокирован #042 (Stage 2); Stage 0 perf-gate можно делать независимо |
| `future/v1_min_ai_config_synthesis_trial_spdlog.md` | Ждёт authoring_rules + eval_protocol |

---

## Без анализа (нужно исследование)

| Файл | Что не хватает |
|------|----------------|
| `new/062_maj_diff_fragment_boundary_align.md` | Stub (~20 строк): цель + идея, нет плана/файлов/критериев. Без движения с 2026-05-31 |
| `new/064_min_diff_vendor_exclude.md` | Stub (~18 строк). Сначала проверить перекрытие с completed #065 (`isGeneratedPath`) и P0.10 vendor-гардом — возможно, уже частично закрыта |
| `new/099_min_indentation_complexity_drift.md` | Не stub, но самостоятельного смысла нет: зафиксировано «только как file-level fallback для #101; если fallback не нужен — закрыть как absorbed» |

---

## Дубли / связанные

| Файлы | Предложение |
|-------|-------------|
| #093 → #094/#095 | #094 и #095 жёстко зависят от shared scanner из #093. Порядок: #093 первым, не параллелить |
| #099 ↔ #101 | #099 — fallback внутри #101; самостоятельно не делать. Возможный исход — закрыть #099 как absorbed |
| #101 ↔ #102 | #102 (прототип) гейтит #101 (продукт); вердикт сейчас `revise` — #101 не стартовать до scorer v2 |
| #029 ↔ #057 | Без изменений с прошлого ревью: #057 реализует метрики, #029 потребляет в diff. Порядок #057 → #029 |
| #074 ↔ #066 | #074 чинит то, что #066 задокументировал; граница по-прежнему не прописана |
| #064 ↔ completed #065/#069 + P0.10 | Возможное перекрытие — проверить до старта |
| #060 ↔ #070 | #060 фактически делегировал остаток в #070; закрывать связкой |
| #071 (future, LD-umbrella) | 4 из 8 идей (LD.10/11/14/16) реализованы и зачёркнуты; живое — LD.15, LD.17, research LD.12/13/18 |

---

## Гигиена нумерации (без изменений с 2026-06-05)

| Номер | Конфликт |
|-------|----------|
| **048** | `new/048_maj_drift_clean_checkout_methodology` ↔ `completed/048_maj_fixture_libresprite_pr581` |
| **071** | `new/071_fp_classification_rules` ↔ `future/071_min_clone_detection_opportunities` |
| **072** | `wip/072_maj_port_056_…` ↔ `completed/072_min_clone_type_classifier` (одна сторона уже в completed — конфликт менее острый) |

---

## TASK_TRACKER.md

Обновлён точечно этим ревью (2026-06-11): #105 переведена из P1 в «сделано» (закрыта сегодня), cheap-drift волна #096/#097/#098/#100 отмечена как реализованная (cb6e09d, лежат в wip до переноса). Стратегические приоритеты P0/P1/P2/OUT не пересматривались — это решение владельца.
