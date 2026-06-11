# Backlog review — 2026-06-11 (второй проход: пересечения и хвосты)

> **Исполнение (вечер 2026-06-11):** рекомендации выполнены. Параллельным проходом `2c76f60`
> закрыты #056/#060/#061, #083 → future, #072 освежён. Этим проходом: #029/#032/#067 → completed,
> #062/#064 → dropped (поглощены), пометки о дублировании в #088/#070/future-090, TASK_TRACKER
> обновлён. Таблицы ниже — снапшот ДО переносов.

Snapshot: `new/` (18), `wip/` (9), `future/` (13) = **40 активных**; `dropped/` (1), кросс-сверка с `completed/` (87) **и с кодом** (`src/`, git log). `pending/` не трогался. Предыдущий снапшот — утро 2026-06-11; его рекомендации частично исполнены (#096/#097/#098/#100/#041/#045/#046/#053/#079/#101/#102/#104 переехали), но он пропустил главный класс проблем — **задачи, уже реализованные в коде**.

## Сводка

- **Протухших по дате нет** (самый старый активный файл — 2026-05-28).
- **4 задачи полностью сделаны в коде, но не закрыты**: #029, #032 (коммиты с их номерами в заголовке!), #061, #067.
- **3 задачи поглощены другими и устарели по конструкции**: #062, #064, #056 (спайк-часть).
- **1 частично сделана в коде**: #088 (3 из 5 пунктов уже в `src/`).
- **2 эстафеты, где старая задача держит wip формально**: #060→#070, #056→#072.
- **1 мёртвый блокер**: #056 «заблокирован #053», а #053 в `dropped/`.
- **1 поправка к утреннему ревью**: `future/090` — «Сделано» не галлюцинация, реализация была и **намеренно откачена** коммитом 4268a39.
- **TASK_TRACKER** числит #032 P0-блокером MVP — он уже реализован.

Итого: **9 из 27 файлов в new/+wip/ можно снять с очереди** переносом/переоформлением, без написания кода.

---

## Сделано в коде, но не закрыто (done-but-not-moved)

Проверено по `src/` и git log, не по словам задач.

| Файл | Доказательство | Рекомендация |
|------|----------------|--------------|
| `new/032_maj_conditional_include_cycles.md` | Коммит `04b523b` «suppress SF.9 on all-conditional include cycles (#032)»: `IncludeDirective.conditional`, трекинг `#if`-глубины в `include_scanner.cpp`, `DependencyGraph::isConditionalEdge`, `allEdgesConditional` в `sf9_no_cycles.cpp:46-57`, тесты в обоих unit-файлах. Все чекбоксы при этом пустые | **Move to completed** (`/fix-issue`). Остаточный хвост: фикстуры `fixtures/sf9/` нет, spdlog-перепрогон не зафиксирован — проверить при закрытии |
| `new/029_maj_metric_regression_detection.md` | Коммит `c480e39` «metric regression detection — chain length, god-headers, NCCD (#029)»: `GraphMetrics` (ccd/acd/nccd/maxChainLength/maxFanIn) в `algorithms.h:23`, `chainLengthGrown`/`newGodHeaders`/`nccdDelta` + `MetricThresholds` в `regression_report.cpp:168-182`. Все чекбоксы пустые | **Move to completed**. При закрытии сверить тест-матрицу из задачи (фикстуры `fixtures/chain_length/` нет) |
| `wip/061_maj_diff_rename_path_validation.md` | Фикс `-M` влит; верификация = iter2 #060 (коммит `0beb2f7`, «фиксы #061+#064») — единственный открытый чекбокс фактически пройден | **Move to completed** (повтор с утреннего ревью — так и не перенесена) |
| `new/067_maj_overnight_eye_verification.md` | Прогон состоялся: `experiments/verification/` (fp_corpus_r2, round2_verdicts), результаты (precision 32→36%) потреблены #060/#070 | **Move to completed** (повтор с утреннего ревью) |

## Поглощены / устарели по конструкции

| Файл | Почему | Рекомендация |
|------|--------|--------------|
| `new/064_min_diff_vendor_exclude.md` | Трижды закрыта: в спайке — iter2-фикс `0beb2f7` (#060); в продукте — vendor-exclude централизован (#069, `project_files.cpp`), generated-skip P0.9 `isGeneratedPath` (#065), whole-file vendored-twin suppression P0.2 (`duplication_scanner.cpp:300,334`) | **Закрыть как absorbed** (#065/#069/#070) |
| `new/062_maj_diff_fragment_boundary_align.md` | FP-segmentation был свойством line-window сегментации спайка #053/#056. Продуктовый `fragmenter.cpp` (#072) изначально режет по границам функций («`{` preceded by `)`», cap 600 — #091) | **Закрыть как obsolete** (поглощена дизайном #072) |
| `wip/056_maj_fast_backend_partial_duplication_pass.md` | Спайк завершён полностью (P1/P3 закрыты, OSS-sweep 19 реп). Остаточные чекбоксы (plumbing, gate semantics, reporters, fixtures) — это ровно скоуп #072, который уже в Phase 2. Блокер «#053 plumbing» мёртв: #053 в `dropped/` как superseded | **Закрыть как completed (спайк) с передачей остатка в #072**; вычистить мёртвый блокер |
| `wip/060_maj_checker_validation_hardening_loop.md` | Iter 1–4 + корпус round2 сделаны; единственный открытый пункт «реализовать confirm-слой» = задача #070, которая в wip с 5 коммитами | **Закрыть by-proxy**, остаток живёт в #070 |

## Частично сделаны — переоформить на остаток

| Файл | Что уже в коде | Живой остаток |
|------|----------------|---------------|
| `new/088_maj_archcheck_fp_from_corpus.md` | №2.1 `is_system_header` (`include_resolver.cpp:41`, комментарий ссылается на #088); №2.2 `isInlineSplitScc` (коммит `8c05878`, «(#088)»); №2.4 generated/vendored guards (#065/#069) | №2.3 (graph `sccs` ↔ SF.9 согласование) + перепрогон PipeWire/legate/acts. Проставить `[x]` сделанным |
| `wip/070_maj_checker_fp_fix_proposals.md` | Все P0-гарды + P1.1/P1.2/P1.4, 46/46 тестов | Measurement-harness (real mapping) + замер precision ≥55% + фитировка порогов. P1.3 НЕ делать — снята вердиктом #083 |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | Phases 1–2 (`854d53c`, `6b4507e`), FP-гарды де-факто влиты через #070 | Reporters (text+json), fixtures, dogfood. Чекбокс «FP-гарды из #070» проставить по факту |
| `new/108_min_post_audit_sweep_rest.md` | Секция 1 закрыта (be56245, в #104) | Секции 2–4: дубли fork/exec, toLowerCopy, JSON-сериализация, lizard-долг — quick win |

## Поправка к утреннему ревью: future/090

Утренний вердикт «секция "Сделано" — артефакт галлюцинации, файлов нет» — **неверен**. Реализация `implicit_state_machine_growth` существовала и была **намеренно откачена** коммитом `4268a39` «revert(rules): убрать implicit_state_machine_growth из основного archcheck» (по итогам #089: YAGNI, нет authority-цитаты). Файл задачи должен фиксировать revert и причину, а не подвергаться вычистке как галлюцинация. **Keep in future**, дописать одну строку про revert.

---

## Живые хвосты в wip/ (реальная незавершённая работа)

| Файл | Остаток | Блокер |
|------|---------|--------|
| `wip/054_maj_ai_repo_duplication_run.md` | shortlist 6 реп, AI-PR→мерж-коммиты matching, within-repo тест, пересчёт сводки | фоновый ×3 lowstar-прогон |
| `wip/066_maj_airepo_remeasure_clonefail.md` | пересбор пула ≥300, доклон dense, harvest2, EXPANSION_REPORT | перемер в полёте; зомби-процессы убивались вручную — проверить, жив ли прогон |
| `wip/070_maj_checker_fp_fix_proposals.md` | measurement + пороги (см. выше) | доступ к `experiments/` (non-hermetic) |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | reporters, fixtures, dogfood | — |
| `wip/083_maj_duplication_precision_gate_grade.md` | — (корректно blocked) | P2 semantic / v0.2 |
| `wip/109_maj_lcx_corpus_validation.md` | весь разбор: per-repo отчёты, глазная проверка, вердикт для #099 | фоновый прогон запущен 2026-06-11 |

## Дубли / пересечения

| Файлы | Предложение |
|-------|-------------|
| #056 ↔ #072 | Остаток #056 = скоуп #072. Закрыть #056, единственным носителем продуктизации оставить #072 |
| #060 ↔ #070 | Эстафета: закрыть #060, носитель — #070 |
| #070 ↔ #083 | #083 сняла P1.3 из #070 — в #070 вычеркнуть, не реализовывать |
| #077 ↔ #103 ↔ #109 | Три копии одного паттерна «walk git history → archcheck --diff → CSV/digest». #109 уже написал работающий скрипт — #077 и #103 строить на его харнесе, не плодить третий обход истории |
| #099 ↔ #109 | Вердикт #099 (file-level fallback vs absorbed) принимать по результатам прогона #109 — не стартовать раньше |
| #093 → #094/#095 | Жёсткая зависимость от shared scanner: #093 первым, не параллелить |
| #074 ↔ #066 | #074 чинит то, что #066 задокументировал; перед стартом #074 дождаться конца перемера #066, иначе будут чинить движущуюся цель |
| #032 ↔ #088 | Пересечение разрешилось в коде: conditional-фильтр (#032) и inline-split (#088 №2.2) — независимые механизмы, оба реализованы |
| #064 ↔ #065/#069/#070 | Поглощена (см. выше) |

## Быстрые победы

| Файл | Цель | Модуль |
|------|------|--------|
| `new/108_min_post_audit_sweep_rest.md` | Секции 2–4 пост-аудитного свипа; конкретные файлы и строки перечислены | SCAN |
| `new/057_maj_lakos_fanout_coupling_checks.md` | `computeGraphMetrics` уже есть — остаток в основном «провод» до отчёта + правило fan-out | GRAPH |
| Переносы (см. выше) | #029, #032, #061, #067 → completed; #062, #064 → закрыть; #056, #060 → закрыть со ссылкой на носителя | — |

## Средние / сложные (без изменений сути с утра)

| Файл | Цель | Блокер |
|------|------|--------|
| `new/073_maj_tech_debt_alignment_cleanup.md` | Umbrella 10 пунктов; крупнейшая продуктовая | сам блокирует #075 |
| `new/075_crt_mvp_v1_trusted_diff_workflow.md` | Канонический PR/diff workflow (critical) | #073 (P0-срез) |
| `new/093_maj_flag_argument.md` | ARG.1/2 + shared scanner (база волны) | — |
| `new/094`, `new/095` | ARG.3/4, config-bag | #093 |
| `new/099_min_indentation_complexity_drift.md` | file-level fallback | вердикт #109 |
| `new/103_maj_copypaste_per_commit_drift.md` | research-прекурсор new-clone-gate | консолидировать с харнесом #109 |
| `new/078_maj_clone_cochange_harm_signal.md` | co-change severity | стабильная база пар (#072) |
| `new/074_maj_ai_repo_discovery_roi_alignment.md` | discovery-контракт (experiments/) | конец перемера #066 |
| `future/*` | без изменений: v1-цепочка ждёт authoring_rules; #050/#052 ждут #042 | — |

## Гигиена

- Коллизии номеров: **071** (`completed/071_fp_classification_rules` ↔ `future/071_min_clone_detection_opportunities`), **072** (`wip/072_maj_port_…` ↔ `completed/072_min_clone_type_classifier`), **048** (обе стороны теперь в completed — снято).
- `wip/109` не имеет git-истории (файл не закоммичен) — закоммитить вместе с очередным чекпойнтом.

## TASK_TRACKER.md

Обновлён точечно этим ревью: вводная заметка о долге трекера переписана по факту (переносы #096–100/#079 состоялись), #032 и #029 отмечены реализованными (04b523b, c480e39), `new/#104` → `completed` (остаток — `new/#108`), `wip/#053` → `dropped/`, вычищены упоминания закрытых #041/#071. Приоритеты P0/P1/P2/OUT не пересматривались.
