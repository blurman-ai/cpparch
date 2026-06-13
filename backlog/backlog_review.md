# Backlog Review — 2026-06-13 (третий проход)

_Все задачи из `new/` и `wip/` прочитаны полностью. `pending/` не трогали._

## Статистика очереди

| Папка | Файлов |
|-------|--------|
| `new/` | 13 |
| `wip/` | 5 |
| `future/` | 14 |
| `dropped/` | 4 |
| `completed/` | 83 |

**Стухших (>30 дней):** 0 — все задачи моложе 12 дней.
**MVP release-блокеры:** 2 (#103 wip; #123 ядро закоммичено 344870f, остаток = parent-guard).

---

## Протухшие / требуют внимания

Нет файлов >30 дней, но два WIP-процесса могут быть стоячими:

| Файл | Последнее git-обновление | Проблема |
|------|--------------------------|---------|
| `wip/066_maj_airepo_remeasure_clonefail.md` | 2026-06-02 (11 дней) | Resumable перемер 16028 CLONEFAIL запущен — статус неизвестен; cron мог не пережить ребут |
| `wip/054_maj_ai_repo_duplication_run.md` | 2026-06-05 (8 дней) | ×3 дрейф-прогон на 160 репах мог зависнуть; `runs_history/lowstar/` не проверен |

**Рекомендация:** проверить `ls /home/localadm/oss/_aidev_state/*.done` и PID cron-скриптов.

---

## MVP Release-блокеры (P0)

| # | Файл | Что нужно | Зависит |
|---|------|-----------|---------|
| **#103** | `wip/103_maj_copypaste_per_commit_drift.md` | Overnight full scan по 185 репам + Шаг 6 (agentic vs human, repo fixed effects) + FINDINGS | — |
| **#123** | `new/123_maj_diff_new_clone_gate.md` | ядро закоммичено 344870f (`detectNewClones` + DRIFT.NEW_CLONE advisory в `--diff`); остаток = parent-guard (клон, который коммит лишь задел) | #103 (пороги) |

**Порядок:** #103 overnight → пороги → #123 parent-guard → MVP тег.

---

## Быстрые победы (< 1 дня)

| # | Файл | Цель | Модуль |
|---|------|------|--------|
| **#088** | `new/088_maj_archcheck_fp_from_corpus.md` | Пункт #2.3: выровнять счётчик `--graph sccs_cyclic` с выводом SF.9; перепрогон 4 FP-реп | SCAN/RULES |
| **#116** | `new/116_min_dup_thin_delegates_tests.md` | Fixtures + 4 Catch2-теста для тонких делегатов (§9.1 duplication_architecture.md, помечен «не проверено») | SCAN/DUP |
| **#108** | `new/108_min_post_audit_sweep_rest.md` | Устранить дублирующиеся блоки (fork/exec, toLowerCopy, JSON-сериализация), опечатки в dup-scanner, lizard-долг | CLEANUP |

---

## Средние задачи (1–3 дня)

| # | Файл | Цель | Модуль | Зависит |
|---|------|------|--------|---------|
| **#057** | `new/057_maj_lakos_fanout_coupling_checks.md` | Правило `Lakos.GodComponentFanOut` + avg-coupling/blast-radius/SCC-size в отчёт — данные в графе уже есть, нужна проводка | GRAPH/RULES | — |
| **#072** | `wip/072_maj_port_056_duplication_into_archcheck.md` | Принять решение по JSON-выводу dup-пар (v0.1 advisory без JSON vs реализовать); закрыть | SCAN | — |
| **#122** | `new/122_maj_grow_corpus_to_1000.md` | Доклонировать остаток worklist ~1209 реп, применить ворота, получить корпус ≥1000 | CORPUS | — |
| **#074** | `new/074_maj_ai_repo_discovery_roi_alignment.md` | Синхронизировать shallow-since + giant-skip между measure и resume; унифицировать CLONEFAIL/skip семантику | RESEARCH/OPS | #066 |

---

## Сложные / долгие (>3 дней)

| # | Файл | Цель | Блокер |
|---|------|------|--------|
| **#119** | `new/119_maj_unified_per_commit_drift_correlation.md` | Единый per-commit датасет, корреляция Spearman + repo fixed effects | #122 (корпус) |
| **#078** | `new/078_maj_clone_cochange_harm_signal.md` | Severity-ранжирование клонов по inconsistent co-change истории | #054 (данные) |
| **#077** | `new/077_maj_per_commit_graph_drift_export.md` | Experiment-скрипт: обход git-истории через archcheck --diff → jsonl drift-сумм | — |
| **#070** | `wip/070_maj_checker_fp_fix_proposals.md` | Measurement-harness: content-hash маппинг пар на fp_corpus_r2.tsv, замер precision-эффекта P0 | — |

---

## Cheap-drift wave (post-MVP P2)

Зависимость: #093 → #094, #095 (shared function_signature_scan substrate):

| # | Файл | Цель |
|---|------|------|
| #093 | `new/093_maj_flag_argument.md` | Bool-флаги в сигнатурах (ARG.1) + bool-литералы на call-сайтах (ARG.2), advisory |
| #094 | `new/094_maj_param_count_and_accretion.md` | >4 параметров (ARG.3) + прирост ≥2 в diff (ARG.4) |
| #095 | `new/095_maj_config_bag_growth.md` | Config-bag struct >15 полей (CFG.1) + прирост ≥3 в diff (CFG.2) |

---

## Дубли / конфликты

| Ситуация | Файлы | Рекомендация |
|----------|-------|--------------|
| **ID clash #117/#118** | `completed/117_min_diff_max_added_lines.md` + `completed/117_min_lateral_cycle_backedge_confirm.md` + `new/118_min_diff_max_added_lines.md` | Проверить, не реализована ли #118 уже (feat добавлен в #117); если да — перенести `new/118` в completed/ |
| **Перекрытие #070/#072** | оба про duplication | #070 = precision measurement, #072 = JSON-decision; закрывать вместе, но не сливать |

---

## Задачи без входных данных

| # | Файл | Проблема |
|---|------|---------|
| #077 | `new/077_maj_per_commit_graph_drift_export.md` | Может быть поглощён инфраструктурой #054 — стоит проверить пересечение |
| #078 | `new/078_maj_clone_cochange_harm_signal.md` | Входные данные (co-change история) не готовы — зависит от завершения #054 |

---

## WIP: статус и следующий шаг

| # | Статус | Следующий шаг |
|---|--------|---------------|
| **#103** | Скрипт готов, синтетический тест пройден | Запустить `python3 copypaste_per_commit.py` overnight (185 реп) |
| **#072** | Всё кроме JSON-вывода готово | Принять решение: JSON в v0.1 или нет → закрыть задачу |
| **#070** | P0-гарды shipped, measurement-harness не дописан | Дописать content-hash маппинг, замерить precision → закрыть |
| **#054** | ×3 прогон на 160 репах может быть стоячим | Проверить PID/progress, перезапустить если нужно |
| **#066** | Resumable CLONEFAIL-перемер мог не пережить ребут | Проверить `.done_repos` и cron, перезапустить |

---

## Рекомендуемый порядок работы

```
P0: #103 overnight → #123 parent-guard → MVP тег
         ↓
Quick wins (параллельно): #088, #116, #108
         ↓
Decisions: #072 (JSON v0.1?)
         ↓
P1: #057 (fanout checks) → #070 (precision measurement)
         ↓
Corpus track: #066 проверить → #074 → #122 → #119
         ↓
Cheap-drift wave: #093 → #094 → #095
```

---

## TASK_TRACKER.md: применено в этом проходе

- ✅ **#123** в P0 как wip (ядро закоммичено 344870f, остаток = parent-guard)
- ✅ `#073`/`#075`/`#032`/`#045` помечены закрытыми
- ✅ ID-clash `#117`/`#118` разобран: `#118` (diff_max_added_lines) — дубль
  реализованного `#117`, перенесён в completed/
- ✅ `#088`/`#116` закрыты (реализованы в этом проходе), `#108` Section 3 сделана
