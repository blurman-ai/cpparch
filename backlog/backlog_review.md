# Backlog review — 2026-06-26 (снимок после bool-волны + doc-sync)

**Активных:** 21 (11 `new/` + 10 `wip/`). Пустых шаблонов нет.
Протухших по дате (>30 дн без правок) — 0 (самый старый: #074 от 2026-06-02).
`pending/` не трогали.

> Этот прогон уже применил board-гигиену: #103 и #122 → `completed/` (цели достигнуты);
> #093 → `wip/` (ARG.1 зашиплен c0f37db, в релизе 0.1.0; остаток ARG.2); #093/#094/#095
> получили `Related: #090`. Таблицы ниже отражают состояние ПОСЛЕ этих переносов.

## Быстрые победы

| Файл | Цель | Модуль |
|------|------|--------|
| `new/144_min_diff_unresolved_baseline_exit2.md` | нерезолвящийся `--diff` ref → exit 2 вместо тихого пустого дерева (ложный gate); фикстура + интеграционный тест | CLI |
| `new/143_maj_diff_shallow_baseline_fetch_ci.md` | убрать `fetch-depth: 0` из рекомендуемого `--diff` CI-сниппета; правки только workflow + 2 дока | CI/DOCS |
| `wip/093_maj_flag_argument.md` (остаток) | ARG.2 (call sites: ≥2 `true`/`false` в одном вызове) поверх уже-зашипленного ARG.1 | SCAN |

## Средние

| Файл | Цель | Модуль | Сложность |
|------|------|--------|-----------|
| `new/057_maj_lakos_fanout_coupling_checks.md` | `Lakos.GodComponentFanOut` + report-only метрики (avg coupling, blast radius, SCC size) | GRAPH/RULES/REPORT | medium |
| `new/094_maj_param_count_and_accretion.md` | `ARG.3/ARG.4` длинный список параметров + accretion; reuse scanner из #093 | SCAN | medium |
| `new/095_maj_config_bag_growth.md` | `CFG.1/CFG.2` config-bag growth (>15 полей / прирост); новый `type_body_scan` | SCAN | medium |
| `new/125_maj_scan_extensionless_headers.md` | сканер не видит extensionless headers (`<vector>`, `gsl/span`) → ложное «0 нарушений» | SCAN | medium |
| `new/126_maj_sf9_collapse_impl_into_component.md` | SF.9: схлопывать header+impl в один Lakos-компонент до детекции циклов (mlpack 259→16 FP) | RULES | medium |
| `wip/127_maj_vendor_generated_exclusion.md` | исключать vendored/generated; скоуп A+B готов, остаток — generic nested-LICENSE, `.gitmodules`, фикстуры | SCAN | medium |
| `wip/133_maj_first_run_noise_floor.md` (остаток) | ядро (advisory в check-mode, gate только SF.9) зашиплено 67a5a2c; остаток — узкий `--diff` mass-rename guard | RULES/CLI | medium |

## Hard / исследовательские

| Файл | Цель | Модуль |
|------|------|--------|
| `wip/054_maj_ai_repo_duplication_run.md` | корпусный прогон граф+дубли по AI-репам | RESEARCH |
| `wip/066_maj_airepo_remeasure_clonefail.md` | перемер CLONEFAIL + расширение корпуса ≥300 | RESEARCH/SCAN |
| `wip/070_maj_checker_fp_fix_proposals.md` | 6 P0 FP-гардов готовы; остаток — measurement-harness + precision на корпусе | SCAN |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | порт #056 готов (CLI `--duplication`, 17 тестов); остаток — JSON-вывод пар (спорно для v0.1) | SCAN |
| `wip/123_maj_diff_new_clone_gate.md` | core `DRIFT.NEW_CLONE` shipped + E2E; остаток — GitHub demo-репо + severity-решение | SCAN |
| `wip/124_maj_corpus_validate_new_clone_gate.md` | продуктовые precision/recall new-clone-gate; остаток — Part B summary | SCAN |
| `wip/129_maj_unify_source_scan_one_pass.md` | единый `AuthoredScope` (one-pass); ядро закоммичено, остаток — шаг 7 (golden, ведётся в #131) | SCAN |
| `wip/131_maj_fresh_corpus_remeasure.md` | мастер корпус-верификатор всей волны сканера; Группа 1 done, Группы 2–6 ждут | RESEARCH |
| `new/074_maj_ai_repo_discovery_roi_alignment.md` | свести discovery-техдолг (ROI-фильтры, giant-skip, resumable) | RESEARCH |
| `new/077_maj_per_commit_graph_drift_export.md` | per-commit export include-графа для ручной верификации drift-кейсов | RESEARCH |
| `new/078_maj_clone_cochange_harm_signal.md` | severity клонов по inconsistent co-change в git-истории (надстройка над #054) | SCAN/RESEARCH |

## Дубли / слияния (не сливать — связки)

| Файлы | Вывод |
|-------|-------|
| #093 / #094 / #095 vs bool-волна #090/#135 | **НЕ дубли, не поглощены** — bool-аргументы / число параметров / config-поля ⟂ bool-поля классов. Общее — только методология. `Related: #090` проставлен. Порядок: #093(остаток)→#094→#095. |
| #123 / #124 | связка: #123 продуктовый вход (shipped), #124 корпусная валидация его precision/recall. |
| #127 / #129 | связка: #129 единый one-pass, #127 точность vendored/generated-предиката в этом проходе. Закрывать вместе через #131. |
| #054 / #066 / #124 / #131 | сходятся на #131 как зонтике-верификаторе (`Верификация: #131` в каждой). |
| #054 / #066 / #072 / #070 / #078 | duplication-семья: порт → FP-гарды → co-change severity → корпус-инфра. Цепочка, оставить. |

## Заблокированные (де-факто разблокированы)

| Файл | Чем заблокирован | Реальность |
|------|------------------|-----------|
| `wip/129` | #131 (шаг 7 golden) | живой блок (Группы 2–6 #131) |
| `wip/124` | #123 | core #123 shipped → не реальный блок |

## Итог

- **21 активная**, все с реальным анализом; протухших по дате — 0.
- **Board-гигиена применена:** #103, #122 → completed; #093 → wip (ARG.1 в релизе, остаток ARG.2).
- **Узкое место волны сканера** — корпус-верификатор #131 (Группы 2–6), через него закрывается #054/#066/#124/#129.
- **Три быстрые победы первыми:** (1) #144 exit-2 на нерезолвящемся ref; (2) #143 shallow base-fetch; (3) #093 ARG.2 call-sites.
